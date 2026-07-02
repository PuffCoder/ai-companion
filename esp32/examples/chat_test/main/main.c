/*
 * chat_test — 完整语音对话闭环（无扬声器，回复打印到串口）
 *
 *   喊 "Hi ESP" → 录音5秒 → STT识别 → GPT回复 → 串口打印
 *
 * 接线：INMP441  VDD→3.3V GND→GND L/R→GPIO21(软件拉低) SCK→GPIO1 WS→GPIO2 SD→GPIO42
 *
 * 准备：
 *   - menuconfig 选 Hi ESP (wn9_hiesp)
 *   - 自定义分区表（model 分区）
 *   - 证书 bundle 完整 + cross-signed（见 sdkconfig.defaults）
 *   - config.h 填 WiFi / API Key
 */
#include <stdio.h>
#include <string.h>
#include <strings.h>   /* strcasecmp */
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"

#include "model_path.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_config.h"

#include "stt.h"
#include "llm.h"
#include "memory.h"
#include "config.h"

static const char *TAG = "chat";
/* 人设模板在 memory.h（PERSONA_MINION/BUTLER/FRIEND），存 NVS */

/* ── INMP441 ─────────────────────────────────── */
#define INMP441_BCLK  1
#define INMP441_WS    2
#define INMP441_DIN   42
#define INMP441_LR    21    /* L/R → 软件拉低=左声道（省一个 GND 脚）*/
#define SAMPLE_RATE   16000
#define MIC_SHIFT     11

/* VAD 自动停录参数 */
#define REC_MAX_SAMPLES  (SAMPLE_RATE * 8)   /* 最长 8 秒 */
#define REC_MIN_SAMPLES  (SAMPLE_RATE * 1)   /* 最短 1 秒（防误触）*/
#define VAD_SILENCE_MS   500                 /* 停嘴 500ms 算说完 */
#define VOICE_THRESHOLD  300                 /* 平均音量低于此值=噪声，不发 Whisper */

static i2s_chan_handle_t s_rx;

/* ── WiFi ────────────────────────────────────── */
static EventGroupHandle_t s_eg;
#define CONNECTED_BIT BIT0

static void wifi_evt(void *a, esp_event_base_t b, int32_t id, void *d)
{
    if (b == WIFI_EVENT && id == WIFI_EVENT_STA_START)            esp_wifi_connect();
    else if (b == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) esp_wifi_connect();
    else if (b == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = d;
        ESP_LOGI(TAG, "WiFi OK: " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_eg, CONNECTED_BIT);
    }
}

static void wifi_init(void)
{
    s_eg = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_evt, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_evt, NULL, NULL);
    wifi_config_t wc = { .sta = { .ssid = WIFI_SSID, .password = WIFI_PASSWORD } };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "连接 WiFi...");
    xEventGroupWaitBits(s_eg, CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}

/* ── I2S 麦克风 ──────────────────────────────── */
static void mic_init(void)
{
    /* L/R 用 GPIO 输出 0 选左声道（代替接 GND，省一个地脚）*/
    gpio_config_t lr = { .pin_bit_mask = 1ULL << INMP441_LR, .mode = GPIO_MODE_OUTPUT };
    ESP_ERROR_CHECK(gpio_config(&lr));
    gpio_set_level(INMP441_LR, 0);

    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&cc, NULL, &s_rx));
    i2s_std_config_t sc = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, .bclk = INMP441_BCLK,
            .ws = INMP441_WS, .dout = I2S_GPIO_UNUSED, .din = INMP441_DIN,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    sc.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &sc));
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));
}

static void mic_read(int16_t *out, int want)
{
    static int32_t raw[512];
    int got = 0;
    while (got < want) {
        int need = want - got;
        if (need > 512) need = 512;
        size_t br = 0;
        i2s_channel_read(s_rx, raw, need * sizeof(int32_t), &br, portMAX_DELAY);
        int n = br / sizeof(int32_t);
        for (int i = 0; i < n; i++) {
            int v = raw[i] >> MIC_SHIFT;
            if (v > 32767)  v = 32767;
            if (v < -32768) v = -32768;
            out[got++] = (int16_t)v;
        }
    }
}

/* 流式回调：逐字打印；遇到 '[' (记忆标记开始) 就停止打印 */
static void on_delta(const char *delta, void *arg)
{
    bool *stop = (bool *)arg;
    if (*stop) return;
    const char *br = strchr(delta, '[');
    if (br) {
        printf("%.*s", (int)(br - delta), delta);   /* 只打印标记前的部分 */
        *stop = true;
    } else {
        printf("%s", delta);
    }
    fflush(stdout);
}

/* ── 主对话任务（大栈：AFE + TLS）─────────────── */
static void chat_task(void *arg)
{
    /* 唤醒词 AFE */
    srmodel_list_t *models = esp_srmodel_init("model");
    afe_config_t *acfg = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    const esp_afe_sr_iface_t *afe = esp_afe_handle_from_config(acfg);
    esp_afe_sr_data_t *afe_data = afe->create_from_config(acfg);
    int chunk = afe->get_feed_chunksize(afe_data);
    int nch   = afe->get_feed_channel_num(afe_data);
    int16_t *fbuf = malloc(chunk * nch * sizeof(int16_t));

    /* 对话用缓冲（PSRAM）*/
    int16_t *pcm = heap_caps_malloc(REC_MAX_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    char text[256], reply[256];
    const int silence_limit = SAMPLE_RATE * VAD_SILENCE_MS / 1000;

    /* 记忆规则：让 GPT 把值得长期记的事实用标记输出 */
    #define MEMO_RULE "\nWhen the user shares personal info worth remembering " \
        "long-term (name / preferences / important people or things), output it " \
        "on a new line at the very end as [remember]short fact[/remember]. " \
        "Do not repeat info you already know."

    /* 系统提示 = 人设(从NVS) + 长期记忆(用户信息) + 记忆规则 */
    char persona[PERSONA_MAX];
    char user_info[USER_MEMORY_MAX];
    char sysprompt[PERSONA_MAX + USER_MEMORY_MAX + 512];  /* +512: 容纳较长的英文记忆规则 */

    if (!memory_get_persona(persona, sizeof(persona))) {
        snprintf(persona, sizeof(persona), "%s", PERSONA_DEFAULT);
    }
    ESP_LOGI(TAG, "Persona: %s", persona);

    if (memory_get_user(user_info, sizeof(user_info))) {
        snprintf(sysprompt, sizeof(sysprompt),
                 "%s\nKnown about the user: %s\nNaturally show that you know them.%s",
                 persona, user_info, MEMO_RULE);
        ESP_LOGI(TAG, "Loaded memory: %s", user_info);
    } else {
        snprintf(sysprompt, sizeof(sysprompt), "%s%s", persona, MEMO_RULE);
    }
    llm_init(sysprompt);

    ESP_LOGI(TAG, "=== Chat ready ===");
    ESP_LOGI(TAG, "Say \"Hi ESP\" to wake, then talk\n");

    while (1) {
        /* ── 外层：等唤醒词 ────────────────────── */
        mic_read(fbuf, chunk * nch);
        afe->feed(afe_data, fbuf);
        afe_fetch_result_t *res = afe->fetch(afe_data);
        if (!res || res->wakeup_state != WAKENET_DETECTED) continue;

        ESP_LOGI(TAG, "🔔 Woke up! Continuous chat (say 'bye' or stay silent to end)");

        /* ── 内层：连续对话 ────────────────────── */
        int idle = 0;                  /* 连续没说话次数 */
        while (1) {
            ESP_LOGI(TAG, "🎙  Speak...");

            /* 录音（VAD 自动停）*/
            int rec = 0, silence = 0;
            bool started = false;
            while (rec < REC_MAX_SAMPLES) {
                mic_read(fbuf, chunk * nch);
                afe->feed(afe_data, fbuf);
                afe_fetch_result_t *r = afe->fetch(afe_data);
                if (!r || r->data_size <= 0) continue;

                int n = r->data_size / sizeof(int16_t);
                if (rec + n > REC_MAX_SAMPLES) n = REC_MAX_SAMPLES - rec;
                memcpy(pcm + rec, r->data, n * sizeof(int16_t));
                rec += n;

                if (r->vad_state == VAD_SPEECH) {
                    started = true;
                    silence = 0;
                } else if (started) {
                    silence += n;
                }
                if (started && silence >= silence_limit && rec >= REC_MIN_SAMPLES) break;
            }

            /* 没开口 → 计数，连续 3 次结束对话 */
            if (!started) {
                if (++idle >= 3) {
                    ESP_LOGI(TAG, "(No speech, chat ended. Say Hi ESP to start again)\n");
                    break;
                }
                continue;
            }

            /* 音量门限：算平均振幅，太低=噪声/静音，跳过不发 Whisper（防幻觉）*/
            int64_t sum = 0;
            for (int i = 0; i < rec; i++) sum += (pcm[i] < 0 ? -pcm[i] : pcm[i]);
            int avg_amp = rec ? (int)(sum / rec) : 0;

            float rec_sec = (float)rec / SAMPLE_RATE;
            ESP_LOGI(TAG, "Recorded %.1fs (vol %d), transcribing...", rec_sec, avg_amp);

            if (avg_amp < VOICE_THRESHOLD) {
                ESP_LOGW(TAG, "Volume too low (noise/silence), skipped");
                if (++idle >= 3) {
                    ESP_LOGI(TAG, "(No speech, chat ended. Say Hi ESP to start again)\n");
                    break;
                }
                continue;
            }
            idle = 0;   /* 有效说话，计数清零 */

            /* STT */
            int64_t t0 = esp_timer_get_time();
            bool stt_ok = stt_transcribe_pcm(pcm, rec, SAMPLE_RATE,
                                             OPENAI_API_KEY, "en", text, sizeof(text));
            float stt_sec = (esp_timer_get_time() - t0) / 1e6f;
            if (!stt_ok || strlen(text) == 0) {
                ESP_LOGW(TAG, "Didn't catch that, please repeat");
                continue;
            }
            ESP_LOGI(TAG, "👤 You: %s", text);

            /* Exit keywords (whole-sentence match to avoid mid-sentence false exit) */
            if (strcasecmp(text, "bye") == 0 || strcasecmp(text, "bye.") == 0 ||
                strcasecmp(text, "goodbye") == 0 || strcasecmp(text, "goodbye.") == 0 ||
                strcasecmp(text, "bye bye") == 0 || strcasecmp(text, "bye-bye.") == 0) {
                ESP_LOGI(TAG, "👋 Bye! (say Hi ESP to start again)\n");
                break;
            }

            /* GPT（流式：边生成边打印）*/
            int64_t t1 = esp_timer_get_time();
            bool stop_print = false;
            printf("🤖 AI: ");
            bool llm_ok = llm_chat_stream(OPENAI_API_KEY, text,
                                          on_delta, &stop_print, reply, sizeof(reply));
            printf("\n");
            float llm_sec = (esp_timer_get_time() - t1) / 1e6f;

            if (!llm_ok) {
                ESP_LOGW(TAG, "No reply from GPT");
            } else {
                /* Extract [remember]fact[/remember] → save to long-term memory */
                char *m1 = strstr(reply, "[remember]");
                if (m1) {
                    char *m2 = strstr(m1, "[/remember]");
                    if (m2) {
                        *m2 = '\0';
                        char *fact = m1 + strlen("[remember]");
                        if (strlen(fact) > 0) {
                            memory_append_user(fact);
                            ESP_LOGI(TAG, "🧠 Remembered: %s", fact);
                            if (memory_get_user(user_info, sizeof(user_info))) {
                                snprintf(sysprompt, sizeof(sysprompt),
                                    "%s\nKnown about the user: %s\nNaturally show that you know them.%s",
                                    persona, user_info, MEMO_RULE);
                                llm_set_system(sysprompt);
                            }
                        }
                    }
                }
            }

            ESP_LOGI(TAG, "⏱  Rec %.1fs | STT %.1fs | GPT %.1fs\n",
                     rec_sec, stt_sec, llm_sec);
            /* 回到内层循环开头，直接听下一句（不用再喊 Hi ESP）*/
        }
    }
}

void app_main(void)
{
    memory_init();   /* 初始化 NVS（长期记忆）*/

    /* 首次开机预填（已存过就不覆盖，测试持久化）*/
    char tmp[USER_MEMORY_MAX];
    if (!memory_get_user(tmp, sizeof(tmp))) {
        memory_set_user("The user's name is David, 28, lives in Lower Hutt, "
                        "works as an engineer, enjoys skiing and riding motorcycles.");
        ESP_LOGI(TAG, "First run: preloaded user info");
    }
    if (!memory_get_persona(tmp, sizeof(tmp))) {
        memory_set_persona(PERSONA_DEFAULT);
        ESP_LOGI(TAG, "First run: preloaded default persona");
    }

    wifi_init();
    mic_init();
    xTaskCreate(chat_task, "chat_task", 16384, NULL, 5, NULL);
}

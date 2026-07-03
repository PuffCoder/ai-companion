/*
 * AI Companion — 整机主程序
 *
 * 编排：唤醒词 → 对话(录音→STT→GPT→打串口) + 表情屏 + 人脸跟随 + LED
 * 状态机 app_state 驱动表情/LED；对话循环推动状态；UART 收 K230 face_x 转头。
 *
 * 语音逻辑沿用 chat_test（跑通版）；暂无喇叭，回复打串口。
 * 硬件引脚见 bsp_board.h。凭证在 main/config.h（自建，gitignore）。
 */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <math.h>
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
#include "cJSON.h"

#include "model_path.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_config.h"

#include "bsp_board.h"
#include "stt.h"
#include "llm.h"
#include "memory.h"
#include "app_state.h"
#include "emotion_engine.h"
#include "servo_driver.h"
#include "uart_bridge.h"
#include "led_driver.h"
#include "config.h"

static const char *TAG = "main";

/* ── 音频参数 ─────────────────────────────────── */
#define SAMPLE_RATE      16000
#define MIC_SHIFT        11
#define REC_MAX_SAMPLES  (SAMPLE_RATE * 8)
#define REC_MIN_SAMPLES  (SAMPLE_RATE * 1)
#define VAD_SILENCE_MS   500
#define VOICE_THRESHOLD  300

/* ── 人脸跟随参数（现场调）───────────────────────── */
#define TRACK_SIGN      (+1)     /* 头越跟越跑就改 -1 */
#define TRACK_GAIN       35
#define TRACK_DEADZONE   0.12f
#define TRACK_MAX_SPEED  60

static i2s_chan_handle_t s_rx;

/* ════════════ WiFi ════════════ */
static EventGroupHandle_t s_eg;
#define CONNECTED_BIT BIT0

static void wifi_evt(void *a, esp_event_base_t b, int32_t id, void *d)
{
    if (b == WIFI_EVENT && id == WIFI_EVENT_STA_START)             esp_wifi_connect();
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
    ESP_LOGI(TAG, "Connecting WiFi...");
    xEventGroupWaitBits(s_eg, CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}

/* ════════════ 麦克风（INMP441 I2S）════════════ */
static void mic_init(void)
{
    gpio_config_t lr = { .pin_bit_mask = 1ULL << BSP_MIC_LR_GPIO, .mode = GPIO_MODE_OUTPUT };
    ESP_ERROR_CHECK(gpio_config(&lr));
    gpio_set_level(BSP_MIC_LR_GPIO, 0);            /* L/R=0 选左声道 */

    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&cc, NULL, &s_rx));
    i2s_std_config_t sc = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, .bclk = BSP_MIC_I2S_BCLK,
            .ws = BSP_MIC_I2S_WS, .dout = I2S_GPIO_UNUSED, .din = BSP_MIC_I2S_DIN,
            .invert_flags = { false, false, false },
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

/* ════════════ 人脸跟随（UART 回调）════════════ */
static void on_face_line(const char *line)
{
    cJSON *root = cJSON_Parse(line);
    if (!root) return;
    const cJSON *t = cJSON_GetObjectItem(root, "t");
    if (cJSON_IsString(t)) {
        if (strcmp(t->valuestring, "ping") == 0) {
            uart_bridge_send_json("{\"t\":\"pong\"}");
        } else if (strcmp(t->valuestring, "noface") == 0) {
            servo_stop();
        } else if (strcmp(t->valuestring, "face") == 0) {   /* 任何状态都跟脸——一直看着你 */
            const cJSON *jx = cJSON_GetObjectItem(root, "x");
            double x = jx ? jx->valuedouble : 0.0;
            if (fabs(x) < TRACK_DEADZONE) {
                servo_stop();
            } else {
                int sp = (int)(TRACK_SIGN * x * TRACK_GAIN);
                if (sp >  TRACK_MAX_SPEED) sp =  TRACK_MAX_SPEED;
                if (sp < -TRACK_MAX_SPEED) sp = -TRACK_MAX_SPEED;
                servo_rotate(sp);
            }
        }
    }
    cJSON_Delete(root);
}

/* ════════════ 表情：跟随状态 ════════════ */
static void ui_task(void *arg)
{
    int last = -1;
    int64_t next_blink = 0;
    while (1) {
        device_state_t s = app_state_get();
        if ((int)s != last) {
            switch (s) {
                case STATE_IDLE:       emotion_set(EMO_IDLE);      break;
                case STATE_LISTENING:  emotion_set(EMO_LISTENING); break;
                case STATE_PROCESSING: emotion_set(EMO_THINKING);  break;
                case STATE_SPEAKING:   emotion_set(EMO_HAPPY);     break;
            }
            last = s;
        }
        /* 待机时偶尔眨眼 */
        if (s == STATE_IDLE && esp_timer_get_time() > next_blink) {
            emotion_blink();
            next_blink = esp_timer_get_time() + 4000000;   /* 4s */
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* ════════════ LED：跟随状态 ════════════ */
static void led_task(void *arg)
{
    while (1) {
        switch (app_state_get()) {
            case STATE_IDLE:       led_driver_set_rgb(0, 16, 0);   break;  /* 暗绿 */
            case STATE_LISTENING:  led_driver_set_rgb(0, 0, 40);   break;  /* 蓝 */
            case STATE_PROCESSING: led_driver_set_rgb(40, 24, 0);  break;  /* 黄 */
            case STATE_SPEAKING:   led_driver_set_rgb(40, 0, 40);  break;  /* 品红 */
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* 流式回调：逐字打印，遇 '[' 停（不显示记忆标记）*/
static void on_delta(const char *delta, void *arg)
{
    bool *stop = (bool *)arg;
    if (*stop) return;
    const char *br = strchr(delta, '[');
    if (br) { printf("%.*s", (int)(br - delta), delta); *stop = true; }
    else    { printf("%s", delta); }
    fflush(stdout);
}

/* ════════════ 主对话任务（大栈：AFE + TLS）════════════ */
#define MEMO_RULE "\nWhen the user shares personal info worth remembering " \
    "long-term (name / preferences / important people or things), output it " \
    "on a new line at the very end as [remember]short fact[/remember]. " \
    "Do not repeat info you already know."

static char persona[PERSONA_MAX];
static char user_info[USER_MEMORY_MAX];
static char sysprompt[PERSONA_MAX + USER_MEMORY_MAX + 512];

static void refresh_sysprompt(void)
{
    if (memory_get_user(user_info, sizeof(user_info)))
        snprintf(sysprompt, sizeof(sysprompt),
                 "%s\nKnown about the user: %s\nNaturally show that you know them.%s",
                 persona, user_info, MEMO_RULE);
    else
        snprintf(sysprompt, sizeof(sysprompt), "%s%s", persona, MEMO_RULE);
}

static void conversation_task(void *arg)
{
    srmodel_list_t *models = esp_srmodel_init("model");
    afe_config_t *acfg = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    const esp_afe_sr_iface_t *afe = esp_afe_handle_from_config(acfg);
    esp_afe_sr_data_t *afe_data = afe->create_from_config(acfg);
    int chunk = afe->get_feed_chunksize(afe_data);
    int nch   = afe->get_feed_channel_num(afe_data);
    int16_t *fbuf = malloc(chunk * nch * sizeof(int16_t));
    int16_t *pcm  = heap_caps_malloc(REC_MAX_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    char text[256], reply[256];
    const int silence_limit = SAMPLE_RATE * VAD_SILENCE_MS / 1000;

    if (!memory_get_persona(persona, sizeof(persona)))
        snprintf(persona, sizeof(persona), "%s", PERSONA_DEFAULT);
    refresh_sysprompt();
    llm_init(sysprompt);

    ESP_LOGI(TAG, "=== Ready === say \"Hi ESP\" to wake");

    while (1) {
        /* 外层：待机等唤醒 */
        app_state_set(STATE_IDLE);
        mic_read(fbuf, chunk * nch);
        afe->feed(afe_data, fbuf);
        afe_fetch_result_t *res = afe->fetch(afe_data);
        if (!res || res->wakeup_state != WAKENET_DETECTED) continue;

        ESP_LOGI(TAG, "Woke up! (say 'bye' or stay silent to end)");
        int idle = 0;

        while (1) {
            app_state_set(STATE_LISTENING);
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
                if (r->vad_state == VAD_SPEECH) { started = true; silence = 0; }
                else if (started) silence += n;
                if (started && silence >= silence_limit && rec >= REC_MIN_SAMPLES) break;
            }

            if (!started) {
                if (++idle >= 3) { ESP_LOGI(TAG, "(no speech, back to idle)"); break; }
                continue;
            }

            int64_t sum = 0;
            for (int i = 0; i < rec; i++) sum += (pcm[i] < 0 ? -pcm[i] : pcm[i]);
            int avg = rec ? (int)(sum / rec) : 0;
            if (avg < VOICE_THRESHOLD) {
                ESP_LOGW(TAG, "too quiet, skipped");
                if (++idle >= 3) { ESP_LOGI(TAG, "(no speech, back to idle)"); break; }
                continue;
            }
            idle = 0;

            app_state_set(STATE_PROCESSING);
            if (!stt_transcribe_pcm(pcm, rec, SAMPLE_RATE, OPENAI_API_KEY, "en", text, sizeof(text))
                || strlen(text) == 0) {
                ESP_LOGW(TAG, "didn't catch that");
                continue;
            }
            ESP_LOGI(TAG, "You: %s", text);

            if (strcasecmp(text, "bye") == 0 || strcasecmp(text, "bye.") == 0 ||
                strcasecmp(text, "goodbye") == 0 || strcasecmp(text, "goodbye.") == 0) {
                ESP_LOGI(TAG, "Bye!");
                break;
            }

            app_state_set(STATE_SPEAKING);
            bool stop_print = false;
            printf("AI: ");
            bool ok = llm_chat_stream(OPENAI_API_KEY, text, on_delta, &stop_print, reply, sizeof(reply));
            printf("\n");
            if (ok) {
                char *m1 = strstr(reply, "[remember]");
                if (m1) {
                    char *m2 = strstr(m1, "[/remember]");
                    if (m2) {
                        *m2 = '\0';
                        char *fact = m1 + strlen("[remember]");
                        if (strlen(fact) > 0) {
                            memory_append_user(fact);
                            ESP_LOGI(TAG, "Remembered: %s", fact);
                            refresh_sysprompt();
                            llm_set_system(sysprompt);
                        }
                    }
                }
            }
        }
    }
}

/* ════════════ app_main：只做初始化 + 起任务 ════════════ */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    memory_init();
    char tmp[USER_MEMORY_MAX];
    if (!memory_get_user(tmp, sizeof(tmp)))
        memory_set_user("The user's name is David, 28, lives in Lower Hutt, "
                        "works as an engineer, enjoys skiing and riding motorcycles.");
    if (!memory_get_persona(tmp, sizeof(tmp)))
        memory_set_persona(PERSONA_DEFAULT);

    app_state_init();
    led_driver_init();
    emotion_engine_init();
    servo_driver_init();
    servo_stop();
    uart_bridge_init();
    uart_bridge_start_rx(on_face_line);
    wifi_init();
    mic_init();

    xTaskCreate(ui_task,  "ui",  4096, NULL, 4, NULL);
    xTaskCreate(led_task, "led", 2048, NULL, 3, NULL);
    xTaskCreate(conversation_task, "chat", 16384, NULL, 5, NULL);
}

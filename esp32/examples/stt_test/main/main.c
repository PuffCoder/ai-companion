/*
 * stt_test — ESP32-S3 语音转文字测试
 *
 * 流程：INMP441 录音(4秒) → 打包 WAV → 上传 OpenAI Whisper → 打印识别文字
 *
 * 接线同 mic_test：
 *   INMP441  VDD→3.3V  GND→GND  L/R→GND  SCK→GPIO1  WS→GPIO2  SD→GPIO42
 *
 * 凭证填在 config.h（已 gitignore）。需要 PSRAM（见 sdkconfig.defaults）。
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "config.h"

static const char *TAG = "stt_test";

/* ── INMP441 ─────────────────────────────────── */
#define INMP441_BCLK  GPIO_NUM_1
#define INMP441_WS    GPIO_NUM_2
#define INMP441_DIN   GPIO_NUM_42
#define SAMPLE_RATE   16000
#define MIC_SHIFT     11                 /* 增益：小=响，大=轻（验证过 11 较好）*/

#define REC_SECONDS   5                  /* 单次录音时长 */
#define REC_SAMPLES   (SAMPLE_RATE * REC_SECONDS)
#define REC_BYTES     (REC_SAMPLES * 2)  /* int16 */

static i2s_chan_handle_t s_rx;

/* ── WiFi ────────────────────────────────────── */
static EventGroupHandle_t s_wifi_eg;
#define WIFI_CONNECTED_BIT BIT0

static void wifi_evt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)        esp_wifi_connect();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) esp_wifi_connect();
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "WiFi OK: " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_wifi_eg, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init(void)
{
    s_wifi_eg = xEventGroupCreate();
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
    xEventGroupWaitBits(s_wifi_eg, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}

/* ── I2S 麦克风 ──────────────────────────────── */
static void mic_init(void)
{
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&cc, NULL, &s_rx));

    i2s_std_config_t sc = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = INMP441_BCLK,
            .ws   = INMP441_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = INMP441_DIN,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    sc.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;   /* INMP441 L/R=GND → 左声道 */
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &sc));
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));
}

/* 录固定 REC_SECONDS 秒到 pcm16（容量 REC_SAMPLES）*/
static void record(int16_t *pcm16)
{
    static int32_t raw[256];
    int got = 0;
    while (got < REC_SAMPLES) {
        size_t br = 0;
        i2s_channel_read(s_rx, raw, sizeof(raw), &br, portMAX_DELAY);
        int n = br / sizeof(int32_t);
        for (int i = 0; i < n && got < REC_SAMPLES; i++) {
            int v = raw[i] >> MIC_SHIFT;
            if (v > 32767)  v = 32767;
            if (v < -32768) v = -32768;
            pcm16[got++] = (int16_t)v;
        }
    }
}

/* 写 44 字节 WAV 头 */
static void wav_header(uint8_t *h, int data_len)
{
    int   br = SAMPLE_RATE * 2;
    short fmt = 1, ch = 1, ba = 2, bps = 16;
    int   sub1 = 16, sr = SAMPLE_RATE, chunk = 36 + data_len;
    memcpy(h,    "RIFF", 4);  memcpy(h + 4,  &chunk, 4);
    memcpy(h + 8,"WAVE", 4);  memcpy(h + 12, "fmt ", 4);
    memcpy(h + 16, &sub1, 4); memcpy(h + 20, &fmt, 2);
    memcpy(h + 22, &ch, 2);   memcpy(h + 24, &sr, 4);
    memcpy(h + 28, &br, 4);   memcpy(h + 32, &ba, 2);
    memcpy(h + 34, &bps, 2);  memcpy(h + 36, "data", 4);
    memcpy(h + 40, &data_len, 4);
}

/* ── HTTP 响应收集 ───────────────────────────── */
static char *s_resp;
static int   s_resp_len;

static esp_err_t http_evt(esp_http_client_event_t *e)
{
    if (e->event_id == HTTP_EVENT_ON_DATA && s_resp_len < 8000) {
        int copy = e->data_len;
        if (s_resp_len + copy > 8000) copy = 8000 - s_resp_len;
        memcpy(s_resp + s_resp_len, e->data, copy);
        s_resp_len += copy;
    }
    return ESP_OK;
}

/* 上传 WAV 到 Whisper，打印识别结果 */
static void whisper(uint8_t *wav, int wav_len)
{
    const char *bd = "----esp32boundary";
    char p1[200], p2[200], p3[200], foot[64];
    int l1 = snprintf(p1, sizeof(p1),
        "--%s\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-1\r\n", bd);
    int l2 = snprintf(p2, sizeof(p2),
        "--%s\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\nzh\r\n", bd);
    int l3 = snprintf(p3, sizeof(p3),
        "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"rec.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n", bd);
    int lf = snprintf(foot, sizeof(foot), "\r\n--%s--\r\n", bd);

    int total = l1 + l2 + l3 + wav_len + lf;
    uint8_t *body = heap_caps_malloc(total, MALLOC_CAP_SPIRAM);
    if (!body) { ESP_LOGE(TAG, "body 分配失败"); return; }

    int off = 0;
    memcpy(body + off, p1, l1);   off += l1;
    memcpy(body + off, p2, l2);   off += l2;
    memcpy(body + off, p3, l3);   off += l3;
    memcpy(body + off, wav, wav_len); off += wav_len;
    memcpy(body + off, foot, lf); off += lf;

    s_resp = heap_caps_malloc(8192, MALLOC_CAP_SPIRAM);
    s_resp_len = 0;

    char ct[128], auth[256];
    snprintf(ct,   sizeof(ct),   "multipart/form-data; boundary=%s", bd);
    snprintf(auth, sizeof(auth), "Bearer %s", OPENAI_API_KEY);

    esp_http_client_config_t cfg = {
        .url               = "https://api.openai.com/v1/audio/transcriptions",
        .method            = HTTP_METHOD_POST,
        .event_handler     = http_evt,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = 20000,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    esp_http_client_set_header(c, "Authorization", auth);
    esp_http_client_set_header(c, "Content-Type", ct);
    esp_http_client_set_post_field(c, (const char *)body, total);

    ESP_LOGI(TAG, "上传 Whisper (%d bytes)...", total);
    esp_err_t err = esp_http_client_perform(c);

    if (err == ESP_OK) {
        int st = esp_http_client_get_status_code(c);
        s_resp[s_resp_len < 8191 ? s_resp_len : 8191] = 0;
        if (st == 200) {
            cJSON *j = cJSON_Parse(s_resp);
            cJSON *t = j ? cJSON_GetObjectItem(j, "text") : NULL;
            if (cJSON_IsString(t)) ESP_LOGI(TAG, "✅ 识别: %s", t->valuestring);
            else                   ESP_LOGW(TAG, "无 text: %s", s_resp);
            cJSON_Delete(j);
        } else {
            ESP_LOGE(TAG, "HTTP %d: %s", st, s_resp);
        }
    } else {
        ESP_LOGE(TAG, "请求失败: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(c);
    heap_caps_free(body);
    heap_caps_free(s_resp);
}

/* 录音 + 识别任务（大栈）*/
static void stt_task(void *arg)
{
    int16_t *pcm = heap_caps_malloc(REC_BYTES, MALLOC_CAP_SPIRAM);
    uint8_t *wav = heap_caps_malloc(44 + REC_BYTES, MALLOC_CAP_SPIRAM);
    if (!pcm || !wav) {
        ESP_LOGE(TAG, "音频缓冲分配失败（PSRAM?）");
        vTaskDelete(NULL);
        return;
    }

    for (int i = 3; i > 0; i--) {
        ESP_LOGI(TAG, "%d 秒后录音...", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "🎤 录音 %d 秒，请说话...", REC_SECONDS);
    record(pcm);
    ESP_LOGI(TAG, "录音完成，识别中...");

    wav_header(wav, REC_BYTES);
    memcpy(wav + 44, pcm, REC_BYTES);
    whisper(wav, 44 + REC_BYTES);

    ESP_LOGI(TAG, "完成。要再录请按 RESET 键。");

    heap_caps_free(pcm);
    heap_caps_free(wav);
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    wifi_init();
    mic_init();

    /* 在大栈任务里跑（HTTP+TLS 吃栈，app_main 默认栈不够）*/
    xTaskCreate(stt_task, "stt_task", 8192, NULL, 5, NULL);
}

/*
 * tts_test — ESP32-S3 文字转语音测试（没有扬声器也能验证）
 *
 * 流程：文字 → OpenAI TTS API → 拿到 PCM 音频 → UDP 传电脑 → 电脑存 WAV 播放
 *
 * 用法：
 *   1. config.h 填 WiFi / API Key / PC_IP（电脑 IP）
 *   2. 电脑先运行：python save_tts.py
 *   3. 烧录本程序，等它请求 TTS 并推流
 *   4. 电脑得到 tts.wav，播放试听
 *
 * 需要 PSRAM + 证书 bundle（见 sdkconfig.defaults）。
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "lwip/sockets.h"
#include "config.h"

static const char *TAG = "tts_test";

/* 要合成的文字（改这里测不同内容）*/
#define TTS_TEXT  "你好，我是小黄人，很高兴认识你！"
#define TTS_VOICE "nova"          /* alloy/echo/nova/shimmer 等 */

/* OpenAI TTS 输出 24kHz；UDP 传到电脑 */
#define TTS_RATE  24000

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

/* ── TCP 推流：把收到的 TTS 音频块可靠地发给电脑 ───── */
static int s_sock = -1;

/* 全部发完（TCP 可能部分发送）*/
static void send_all(const uint8_t *p, int len)
{
    int sent = 0;
    while (sent < len) {
        int w = send(s_sock, p + sent, len - sent, 0);
        if (w <= 0) break;
        sent += w;
    }
}

static esp_err_t http_evt(esp_http_client_event_t *e)
{
    if (e->event_id == HTTP_EVENT_ON_DATA && e->data_len > 0 && s_sock >= 0) {
        send_all((const uint8_t *)e->data, e->data_len);
    }
    return ESP_OK;
}

/* ── TTS 任务（大栈，HTTP+TLS 吃栈）────────────── */
static void tts_task(void *arg)
{
    /* TCP 连到电脑（电脑要先运行 save_tts.py 当服务器）*/
    s_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port   = htons(UDP_PORT),
        .sin_addr.s_addr = inet_addr(PC_IP),
    };
    if (connect(s_sock, (struct sockaddr *)&dest, sizeof(dest)) != 0) {
        ESP_LOGE(TAG, "TCP 连接电脑失败，先运行 save_tts.py");
        close(s_sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "已连接电脑 %s:%d", PC_IP, UDP_PORT);

    /* 请求体 JSON：用 pcm 格式（裸 PCM，无文件头，方便直接传）*/
    char body[512];
    int blen = snprintf(body, sizeof(body),
        "{\"model\":\"tts-1\",\"input\":\"%s\",\"voice\":\"%s\",\"response_format\":\"pcm\"}",
        TTS_TEXT, TTS_VOICE);

    char auth[256];
    snprintf(auth, sizeof(auth), "Bearer %s", OPENAI_API_KEY);

    esp_http_client_config_t cfg = {
        .url               = "https://api.openai.com/v1/audio/speech",
        .method            = HTTP_METHOD_POST,
        .event_handler     = http_evt,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = 30000,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    esp_http_client_set_header(c, "Authorization", auth);
    esp_http_client_set_header(c, "Content-Type", "application/json");
    esp_http_client_set_post_field(c, body, blen);

    ESP_LOGI(TAG, "请求 TTS: \"%s\"", TTS_TEXT);
    esp_err_t err = esp_http_client_perform(c);

    if (err == ESP_OK) {
        int st = esp_http_client_get_status_code(c);
        ESP_LOGI(TAG, "TTS HTTP %d，音频已推流到 %s:%d", st, PC_IP, UDP_PORT);
        if (st != 200) ESP_LOGE(TAG, "TTS 失败，检查 API Key");
    } else {
        ESP_LOGE(TAG, "请求失败: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(c);

    close(s_sock);          /* 关连接 → 电脑收到 EOF，自动保存 */
    s_sock = -1;
    ESP_LOGI(TAG, "完成。pcm %dHz 单声道 16-bit 已发送，电脑自动保存。", TTS_RATE);
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase(); nvs_flash_init();
    }
    wifi_init();
    xTaskCreate(tts_task, "tts_task", 12288, NULL, 5, NULL);
}

/*
 * mic_test — INMP441 录音 → UDP 传到电脑存成 WAV（用来听录音质量）
 *
 * 思路：电脑能听清录音，OpenAI Whisper 就也能识别。
 *
 * 接线：VDD→3.3V GND→GND L/R→GND SCK→GPIO1 WS→GPIO2 SD→GPIO42
 *
 * 用法：
 *   1. 电脑查 IP（ipconfig），填进 config.h 的 PC_IP
 *   2. 电脑先运行：python save_wav.py
 *   3. 烧录本程序，对着麦克风说话
 *   4. 电脑按 Ctrl+C 停止，得到 recording.wav，播放试听
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "driver/i2s_std.h"
#include "lwip/sockets.h"
#include "config.h"

static const char *TAG = "mic_test";

/* ── INMP441 ─────────────────────────────────── */
#define INMP441_BCLK  1
#define INMP441_WS    2
#define INMP441_DIN   42
#define SAMPLE_RATE   16000
#define MIC_SHIFT     11              /* 增益：小=响，大=轻 */
#define CHUNK         256

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
    sc.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;   /* INMP441 L/R=GND → 左声道 */
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &sc));
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));
}

void app_main(void)
{
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase(); nvs_flash_init();
    }
    wifi_init();
    mic_init();

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port   = htons(UDP_PORT),
        .sin_addr.s_addr = inet_addr(PC_IP),
    };
    ESP_LOGI(TAG, "推流到 %s:%d，对着麦克风说话...", PC_IP, UDP_PORT);

    static int32_t raw[CHUNK];
    static int16_t pcm[CHUNK];

    while (1) {
        size_t br = 0;
        i2s_channel_read(s_rx, raw, sizeof(raw), &br, portMAX_DELAY);
        int n = br / sizeof(int32_t);
        for (int i = 0; i < n; i++) {
            int v = raw[i] >> MIC_SHIFT;
            if (v > 32767)  v = 32767;
            if (v < -32768) v = -32768;
            pcm[i] = (int16_t)v;
        }
        sendto(sock, pcm, n * sizeof(int16_t), 0,
               (struct sockaddr *)&dest, sizeof(dest));
    }
}

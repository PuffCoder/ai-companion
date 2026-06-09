/*
 * chat_test — 完整语音对话闭环（无扬声器，回复打印到串口）
 *
 *   喊 "Hi ESP" → 录音5秒 → STT识别 → GPT回复 → 串口打印
 *
 * 接线：INMP441  VDD→3.3V GND→GND L/R→GND SCK→GPIO1 WS→GPIO2 SD→GPIO42
 *
 * 准备：
 *   - menuconfig 选 Hi ESP (wn9_hiesp)
 *   - 自定义分区表（model 分区）
 *   - 证书 bundle 完整 + cross-signed（见 sdkconfig.defaults）
 *   - config.h 填 WiFi / API Key
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

#include "model_path.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_config.h"

#include "stt.h"
#include "llm.h"
#include "config.h"

static const char *TAG = "chat";

/* ── INMP441 ─────────────────────────────────── */
#define INMP441_BCLK  1
#define INMP441_WS    2
#define INMP441_DIN   42
#define SAMPLE_RATE   16000
#define MIC_SHIFT     11
#define REC_SECONDS   5
#define REC_SAMPLES   (SAMPLE_RATE * REC_SECONDS)

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
    int16_t *pcm = heap_caps_malloc(REC_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    char text[256], reply[256];

    llm_init(NULL);   /* 默认人设：小黄人 */

    ESP_LOGI(TAG, "=== 对话就绪 ===");
    ESP_LOGI(TAG, "喊 \"Hi ESP\" 唤醒，然后说话\n");

    while (1) {
        /* 1. 等唤醒词 */
        mic_read(fbuf, chunk * nch);
        afe->feed(afe_data, fbuf);
        afe_fetch_result_t *res = afe->fetch(afe_data);
        if (!res || res->wakeup_state != WAKENET_DETECTED) continue;

        ESP_LOGI(TAG, "🔔 已唤醒！请说话（%d秒）...", REC_SECONDS);

        /* 2. 录音 */
        mic_read(pcm, REC_SAMPLES);
        ESP_LOGI(TAG, "识别中...");

        /* 3. STT */
        if (!stt_transcribe_pcm(pcm, REC_SAMPLES, SAMPLE_RATE,
                                OPENAI_API_KEY, "zh", text, sizeof(text))
            || strlen(text) == 0) {
            ESP_LOGW(TAG, "没听清，再喊 Hi ESP 试试\n");
            continue;
        }
        ESP_LOGI(TAG, "👤 你: %s", text);

        /* 4. GPT */
        if (llm_chat(OPENAI_API_KEY, text, reply, sizeof(reply))) {
            ESP_LOGI(TAG, "🤖 AI: %s\n", reply);
        } else {
            ESP_LOGW(TAG, "GPT 没回复\n");
        }
        /* 5.（无扬声器，回复已打印。以后这里接 TTS 播放）*/
    }
}

void app_main(void)
{
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase(); nvs_flash_init();
    }
    wifi_init();
    mic_init();
    xTaskCreate(chat_task, "chat_task", 16384, NULL, 5, NULL);
}

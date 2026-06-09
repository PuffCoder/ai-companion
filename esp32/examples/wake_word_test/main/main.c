/*
 * wake_word_test — 唤醒词检测（"Hi ESP"）+ INMP441 麦克风
 *
 * 流程：INMP441 录音 → AFE 处理 → WakeNet 检测 → 喊"Hi ESP"触发
 *
 * 接线：VDD→3.3V GND→GND L/R→GND SCK→GPIO1 WS→GPIO2 SD→GPIO42
 *
 * 准备：menuconfig 选 Hi ESP (wn9_hiesp)；自定义分区表（model 分区）。
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2s_std.h"

#include "model_path.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_config.h"

static const char *TAG = "wakeword";

/* ── INMP441 ─────────────────────────────────── */
#define INMP441_BCLK  1
#define INMP441_WS    2
#define INMP441_DIN   42
#define SAMPLE_RATE   16000
#define MIC_SHIFT     11

static i2s_chan_handle_t s_rx;

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

/* 读 want 个 int16 采样（32位→16位缩放）*/
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

void app_main(void)
{
    mic_init();

    /* 加载模型 + 创建 AFE */
    srmodel_list_t *models = esp_srmodel_init("model");
    afe_config_t *cfg = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    const esp_afe_sr_iface_t *afe = esp_afe_handle_from_config(cfg);
    esp_afe_sr_data_t *afe_data = afe->create_from_config(cfg);

    int chunk = afe->get_feed_chunksize(afe_data);
    int nch   = afe->get_feed_channel_num(afe_data);
    int16_t *buf = malloc(chunk * nch * sizeof(int16_t));

    ESP_LOGI(TAG, "就绪！对着麦克风喊 \"Hi ESP\"（chunk=%d ch=%d）", chunk, nch);

    while (1) {
        mic_read(buf, chunk * nch);          /* 读一帧 */
        afe->feed(afe_data, buf);            /* 喂给 AFE */

        afe_fetch_result_t *res = afe->fetch(afe_data);
        if (res && res->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG, "🔔 检测到唤醒词 Hi ESP！");
        }
    }
}

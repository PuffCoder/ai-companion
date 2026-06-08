/*
 * mic_test — INMP441 I2S 录音测试（ESP32-S3）
 *
 * 持续读麦克风，打印平均/峰值音量。对着麦克风说话/敲击看数值跳变。
 *   安静     → 平均小（几十~几百）
 *   说话/敲击 → 平均/峰值明显跳大
 *
 * 接线：
 *   VDD→3.3V  GND→GND  L/R→GND  SCK→GPIO1  WS→GPIO2  SD→GPIO42
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

static const char *TAG = "mic_test";

/* ── INMP441 引脚 ─────────────────────────────── */
#define INMP441_BCLK   1
#define INMP441_WS     2
#define INMP441_DIN    42

#define SAMPLE_RATE    16000
#define MIC_SHIFT      14     /* 32位→16位缩放，小=响，大=轻 */
#define READ_SAMPLES   1024

static i2s_chan_handle_t s_rx;
static int32_t           s_raw[READ_SAMPLES];

static void mic_init(void)
{
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&cc, NULL, &s_rx);

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
    i2s_channel_init_std_mode(s_rx, &sc);
    i2s_channel_enable(s_rx);

    ESP_LOGI(TAG, "I2S 麦克风就绪 (BCLK=%d WS=%d DIN=%d, shift=%d)",
             INMP441_BCLK, INMP441_WS, INMP441_DIN, MIC_SHIFT);
}

void app_main(void)
{
    mic_init();
    ESP_LOGI(TAG, "对着 INMP441 说话/敲击，观察音量变化...");

    while (1) {
        size_t br = 0;
        i2s_channel_read(s_rx, s_raw, sizeof(s_raw), &br, portMAX_DELAY);
        int n = br / sizeof(int32_t);

        int64_t sum = 0;
        int peak = 0;
        for (int i = 0; i < n; i++) {
            int v = (int)(s_raw[i] >> MIC_SHIFT);
            if (v < 0) v = -v;
            sum += v;
            if (v > peak) peak = v;
        }
        int avg = n ? (int)(sum / n) : 0;

        ESP_LOGI(TAG, "音量 平均:%-6d 峰值:%-6d", avg, peak);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

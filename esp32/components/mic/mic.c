#include "mic.h"
#include "bsp_board.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "mic";

/* INMP441 24 位数据在 32 位槽里左对齐。右移缩放到 16 位范围。
 * 越小=越响（增益越大）。11~14 之间调，11≈较响。 */
#define MIC_SHIFT        11
#define MIC_READ_CHUNK   256

static i2s_chan_handle_t s_rx;
static int32_t           s_raw[MIC_READ_CHUNK];

void mic_init(void)
{
    /* L/R 用 GPIO 输出 0 选左声道（省一个 GND 脚）。读音频前先拉低。*/
    gpio_config_t lr = {
        .pin_bit_mask = 1ULL << BSP_MIC_LR_GPIO,
        .mode         = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&lr));
    gpio_set_level(BSP_MIC_LR_GPIO, 0);

    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&cc, NULL, &s_rx));

    i2s_std_config_t sc = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(MIC_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = BSP_MIC_I2S_BCLK,
            .ws   = BSP_MIC_I2S_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = BSP_MIC_I2S_DIN,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    sc.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;   /* L/R=GND → 左声道 */

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &sc));
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));

    ESP_LOGI(TAG, "mic ready (BCLK=%d WS=%d DIN=%d)",
             BSP_MIC_I2S_BCLK, BSP_MIC_I2S_WS, BSP_MIC_I2S_DIN);
}

int mic_read(int16_t *out, int max_samples)
{
    int total = 0;
    while (total < max_samples) {
        int want = max_samples - total;
        if (want > MIC_READ_CHUNK) want = MIC_READ_CHUNK;

        size_t br = 0;
        if (i2s_channel_read(s_rx, s_raw, want * sizeof(int32_t),
                             &br, portMAX_DELAY) != ESP_OK) {
            break;
        }
        int n = br / sizeof(int32_t);
        for (int i = 0; i < n; i++) {
            int v = s_raw[i] >> MIC_SHIFT;
            if (v > 32767)  v = 32767;
            if (v < -32768) v = -32768;
            out[total++] = (int16_t)v;
        }
    }
    return total;
}

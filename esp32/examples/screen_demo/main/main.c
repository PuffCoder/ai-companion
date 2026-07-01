/*
 * screen_demo — 像素风表情演示（ST7789）
 *
 * 屏 3.3V。接线（display_driver.h）：
 *   VCC→3.3V GND→GND
 *   SCLK→GPIO12 MOSI→GPIO11 CS→GPIO10 DC→GPIO9 RST→GPIO8 BLK→GPIO46
 *
 * 依次展示 6 个表情，每个眨两下眼。
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "emotion_engine.h"

static const char *TAG = "screen_demo";

void app_main(void)
{
    ESP_LOGI(TAG, "emotion demo start");
    emotion_engine_init();

    const emotion_t seq[] = {
        EMO_IDLE, EMO_LISTENING, EMO_THINKING,
        EMO_HAPPY, EMO_SAD, EMO_SURPRISED, EMO_ANGRY,
    };
    const char *name[] = {
        "IDLE", "LISTENING", "THINKING", "HAPPY", "SAD", "SURPRISED", "ANGRY",
    };

    while (1) {
        for (int i = 0; i < 7; i++) {
            ESP_LOGI(TAG, "%s", name[i]);
            emotion_set(seq[i]);
            vTaskDelay(pdMS_TO_TICKS(3000));
            emotion_blink();
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    }
}

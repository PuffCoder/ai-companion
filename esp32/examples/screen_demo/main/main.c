#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "display_driver.h"

static const char *TAG = "screen_demo";

void app_main(void)
{
    ESP_LOGI(TAG, "Screen demo start");

    display_driver_init();

    ESP_LOGI(TAG, "Filling RED");
    display_fill(COLOR_RED);
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Filling GREEN");
    display_fill(COLOR_GREEN);
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Filling BLUE");
    display_fill(COLOR_BLUE);
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Filling WHITE");
    display_fill(COLOR_WHITE);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Loop forever cycling colors
    const uint16_t colors[] = { COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_WHITE, COLOR_BLACK };
    int idx = 0;
    while (1) {
        display_fill(colors[idx]);
        idx = (idx + 1) % 5;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led_driver.h"
#include "wifi_manager.h"

/* ── LED task ────────────────────────────────────────────────────
 * 未连接 WiFi: 蓝色慢闪（500ms）
 * 连接成功:    绿色常亮
 * ─────────────────────────────────────────────────────────────── */
static void led_task(void *pv)
{
    bool led_on = false;

    while (1) {
        if (wifi_manager_is_connected()) {
            // 绿色常亮
            led_driver_set_rgb(0, 32, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            // 蓝色慢闪
            led_on = !led_on;
            if (led_on) {
                led_driver_set_rgb(0, 0, 32);
            } else {
                led_driver_off();
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

/* ── Entry point ─────────────────────────────────────────────── */
void app_main(void)
{
    led_driver_init();
    wifi_manager_init();

    xTaskCreate(led_task, "led_task", 2048, NULL, 5, NULL);
}

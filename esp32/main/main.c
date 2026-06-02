#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led_driver.h"
#include "wifi_manager.h"
#include "display_driver.h"

/* ── LED task ────────────────────────────────────────────────────
 * 未连接 WiFi: 蓝色慢闪（500ms）
 * 连接成功:    绿色常亮
 * ─────────────────────────────────────────────────────────────── */
static void led_task(void *pv)
{
    bool led_on = false;

    while (1) {
        if (wifi_manager_is_connected()) {
            led_driver_set_rgb(0, 32, 0);   // 绿色常亮
            vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            led_on = !led_on;
            if (led_on) {
                led_driver_set_rgb(0, 0, 32);  // 蓝色慢闪
            } else {
                led_driver_off();
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

/* ── Display test task ───────────────────────────────────────────
 * 依次显示红、绿、蓝、白，每色停留 1 秒
 * ─────────────────────────────────────────────────────────────── */
static void display_task(void *pv)
{
    const uint16_t colors[] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_WHITE
    };
    int idx = 0;

    while (1) {
        display_fill(colors[idx]);
        // 串口提示当前颜色
        idx = (idx + 1) % 4;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ── Entry point ─────────────────────────────────────────────── */
void app_main(void)
{
    led_driver_init();
    wifi_manager_init();
    display_driver_init();

    xTaskCreate(led_task,     "led_task",     2048, NULL, 5, NULL);
    xTaskCreate(display_task, "display_task", 4096, NULL, 4, NULL);
}

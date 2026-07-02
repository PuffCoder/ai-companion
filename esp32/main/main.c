#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led_driver.h"
#include "wifi_manager.h"
#include "display_driver.h"
#include "console.h"
#include "servo_driver.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"

/* ── Screen layout ───────────────────────────────────────────────
 *  y=  8  "AI Companion"       (title)
 *  y= 40  "WiFi: Connected"    (status line)
 *  y= 60  "IP: 192.168.x.x"   (IP line)
 *  Status bar: 4px bar at bottom
 * ─────────────────────────────────────────────────────────────── */

#define LINE_TITLE   8
#define LINE_STATUS  40
#define LINE_IP      60
#define BAR_Y        (DISPLAY_HEIGHT - 6)

/* ── LED task: blue blink = connecting, green = connected ─────── */
static void led_task(void *pv)
{
    bool led_on = false;
    while (1) {
        if (wifi_manager_is_connected()) {
            led_driver_set_rgb(0, 32, 0);
        } else {
            led_on = !led_on;
            led_on ? led_driver_set_rgb(0, 0, 32) : led_driver_off();
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ── WiFi status task: updates screen every second ───────────── */
static void status_task(void *pv)
{
    /* Draw title once */
    display_draw_string(4, LINE_TITLE, "AI Companion", COLOR_CYAN, COLOR_BLACK);

    char prev_ip[16] = "";
    bool prev_connected = false;

    while (1) {
        bool connected = wifi_manager_is_connected();
        char ip[16];
        wifi_manager_get_ip(ip, sizeof(ip));

        /* Redraw only when state changes */
        if (connected != prev_connected || strcmp(ip, prev_ip) != 0) {

            /* Clear status + IP lines */
            display_fill_rect(0, LINE_STATUS - 2,
                              DISPLAY_WIDTH, FONT_CHAR_H * 2 + 8,
                              COLOR_BLACK);

            if (connected) {
                display_draw_string(4, LINE_STATUS, "WiFi: Connected",
                                    COLOR_GREEN, COLOR_BLACK);
                char ip_line[24];
                snprintf(ip_line, sizeof(ip_line), "IP: %s", ip);
                display_draw_string(4, LINE_IP, ip_line,
                                    COLOR_WHITE, COLOR_BLACK);

                /* Green status bar */
                display_fill_rect(0, BAR_Y, DISPLAY_WIDTH, 6, COLOR_GREEN);
            } else {
                display_draw_string(4, LINE_STATUS, "WiFi: Connecting",
                                    COLOR_YELLOW, COLOR_BLACK);
                display_draw_string(4, LINE_IP, "IP: ---",
                                    COLOR_WHITE, COLOR_BLACK);

                /* Red status bar */
                display_fill_rect(0, BAR_Y, DISPLAY_WIDTH, 6, COLOR_RED);
            }

            prev_connected = connected;
            snprintf(prev_ip, sizeof(prev_ip), "%s", ip);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ── Servo task: sweep to 0° → pause 3s → sweep to 90° → pause 3s ── */
#define SERVO_MIN       0
#define SERVO_MAX       90
#define SERVO_STEP_MS   12    // 每度时间(ms)，控制扫描速度
#define SERVO_PAUSE_MS  3000  // 端点停留时间

static void servo_sweep(int from, int to)
{
    int dir = (to > from) ? 1 : -1;
    for (int a = from; a != to + dir; a += dir) {
        servo_set_angle((uint8_t)a);
        vTaskDelay(pdMS_TO_TICKS(SERVO_STEP_MS));
    }
}

static void servo_task(void *pv)
{
    while (1) {
        servo_sweep(SERVO_MAX, SERVO_MIN);          // 扫到 0°
        vTaskDelay(pdMS_TO_TICKS(SERVO_PAUSE_MS));  // 停 3s

        servo_sweep(SERVO_MIN, SERVO_MAX);          // 扫到 90°
        vTaskDelay(pdMS_TO_TICKS(SERVO_PAUSE_MS));  // 停 3s
    }
}

/* ── Entry point ─────────────────────────────────────────────── */
void app_main(void)
{
    display_driver_init();
    led_driver_init();
    servo_driver_init();
    wifi_manager_init();
    console_init();

    xTaskCreate(led_task,    "led_task",    2048, NULL, 5, NULL);
    xTaskCreate(status_task, "status_task", 3072, NULL, 4, NULL);
    // xTaskCreate(servo_task,  "servo_task",  2048, NULL, 3, NULL);
}

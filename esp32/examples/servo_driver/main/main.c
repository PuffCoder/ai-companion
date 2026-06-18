/*
 * 连续旋转舵机测试 — 用 servo_rotate(speed) 接口
 *
 * servo_rotate(speed): speed -100~+100，0=停，正负=方向，绝对值=快慢
 * 人脸跟踪会用：偏左转一边，偏右转另一边，居中停。
 *
 * 接线：信号→GPIO14（bsp_board.h）  VCC→5V外部  GND→GND
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "servo_driver.h"

static const char *TAG = "cr_servo";

void app_main(void)
{
    servo_driver_init();

    while (1) {
        ESP_LOGI(TAG, "正转 (speed +30)");
        servo_rotate(30);
        vTaskDelay(pdMS_TO_TICKS(1500));

        ESP_LOGI(TAG, "停");
        servo_stop();
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "反转 (speed -30)");
        servo_rotate(-30);
        vTaskDelay(pdMS_TO_TICKS(1500));

        ESP_LOGI(TAG, "停");
        servo_stop();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

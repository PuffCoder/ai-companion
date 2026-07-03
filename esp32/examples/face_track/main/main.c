/*
 * face_track — K230 人脸位置 → 舵机跟随（闭环靠摄像头反馈）
 *
 * K230 经 UART 发 {"t":"face","x":-1..1,"n":..} / {"t":"noface"}
 * 连续旋转舵机按比例转速跟脸：
 *   x>0 脸偏右 → 往右转；x<0 偏左 → 往左转；|x|小 → 停。
 * 头一转，摄像头看到的 face_x 就变，形成闭环（脸回到中间→停）。
 *
 * 接线：UART 见 bsp_board.h（GPIO17/18↔K230）；舵机 GPIO14 + 外部 5V。
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "cJSON.h"
#include "uart_bridge.h"
#include "servo_driver.h"

static const char *TAG = "face_track";

/* ── 跟随参数（现场调）───────────────────────────── */
#define TRACK_SIGN      (+1)     /* 若头"越跟越跑"，改成 -1 反向 */
#define TRACK_GAIN       35      /* 速度 = x * GAIN，越大转越快 */
#define TRACK_DEADZONE   0.12f   /* |x| 小于此值当作居中 → 停 */
#define TRACK_MAX_SPEED  60      /* 转速上限，防甩太快 */

static void track_face(double x)
{
    if (fabs(x) < TRACK_DEADZONE) {
        servo_stop();
        ESP_LOGI(TAG, "face x=%.2f → 居中，停", x);
        return;
    }
    int speed = (int)(TRACK_SIGN * x * TRACK_GAIN);
    if (speed >  TRACK_MAX_SPEED) speed =  TRACK_MAX_SPEED;
    if (speed < -TRACK_MAX_SPEED) speed = -TRACK_MAX_SPEED;
    servo_rotate(speed);
    ESP_LOGI(TAG, "face x=%.2f → 转速 %d (%s)", x, speed, speed > 0 ? "右" : "左");
}

/* 收到一整行 JSON（uart_bridge RX 任务上下文）*/
static void on_line(const char *line)
{
    cJSON *root = cJSON_Parse(line);
    if (!root) return;

    const cJSON *t = cJSON_GetObjectItem(root, "t");
    if (cJSON_IsString(t)) {
        if (strcmp(t->valuestring, "face") == 0) {
            const cJSON *jx = cJSON_GetObjectItem(root, "x");
            track_face(jx ? jx->valuedouble : 0.0);
        } else if (strcmp(t->valuestring, "noface") == 0) {
            servo_stop();
            ESP_LOGI(TAG, "没有人脸 → 停");
        } else if (strcmp(t->valuestring, "ping") == 0) {
            uart_bridge_send_json("{\"t\":\"pong\"}");   /* 保持链路探活 */
        }
    }
    cJSON_Delete(root);
}

void app_main(void)
{
    servo_driver_init();
    servo_stop();
    uart_bridge_init();
    uart_bridge_start_rx(on_line);

    ESP_LOGI(TAG, "人脸跟随就绪，等 K230 的 face_x...");
    /* 主循环无事可做，跟随全在 UART 回调里 */
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}

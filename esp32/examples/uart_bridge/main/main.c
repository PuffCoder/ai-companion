/*
 * K230 ↔ ESP32 UART 桥测试。
 *
 * 接线（见 bsp_board.h，UART1）：
 *   ESP32 TX(GPIO17) → K230 RX
 *   ESP32 RX(GPIO18) ← K230 TX
 *   GND ─ GND（共地）
 *
 * 联调三步（协议见 shared/protocol/protocol.md）：
 *   1) 环回自测：把 GPIO17 和 GPIO18 用杜邦线短接，本程序发 ping
 *      会自己收到、回 pong、再收到 pong —— 证明 UART 驱动 OK。
 *   2) 接 K230：K230 发 {"t":"face","x":..,"n":..}，这里打印解析结果。
 *   3) 反向：本程序每秒发 {"t":"ping"}，K230 收到回 {"t":"pong"}。
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "cJSON.h"
#include "uart_bridge.h"

static const char *TAG = "uart_test";

/* 收到一整行 JSON 时回调（在 RX 任务上下文）*/
static void on_line(const char *line)
{
    cJSON *root = cJSON_Parse(line);
    if (!root) {
        ESP_LOGW(TAG, "非法 JSON，丢弃: %s", line);
        return;
    }

    const cJSON *t = cJSON_GetObjectItem(root, "t");
    if (cJSON_IsString(t)) {
        if (strcmp(t->valuestring, "face") == 0) {
            double x = cJSON_GetObjectItem(root, "x") ? cJSON_GetObjectItem(root, "x")->valuedouble : 0;
            int    n = cJSON_GetObjectItem(root, "n") ? cJSON_GetObjectItem(root, "n")->valueint    : 0;
            ESP_LOGI(TAG, "人脸: x=%.2f n=%d  → 舵机方向: %s",
                     x, n, x > 0.1 ? "右" : (x < -0.1 ? "左" : "中/停"));
        } else if (strcmp(t->valuestring, "ping") == 0) {
            ESP_LOGI(TAG, "收到 ping → 回 pong");
            uart_bridge_send_json("{\"t\":\"pong\"}");
        } else if (strcmp(t->valuestring, "pong") == 0) {
            ESP_LOGI(TAG, "收到 pong（链路通）");
        } else {
            ESP_LOGI(TAG, "其它消息: %s", line);
        }
    }
    cJSON_Delete(root);
}

void app_main(void)
{
    uart_bridge_init();
    uart_bridge_start_rx(on_line);

    ESP_LOGI(TAG, "UART 桥就绪，每秒发一次 ping");
    while (1) {
        uart_bridge_send_json("{\"t\":\"ping\"}");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

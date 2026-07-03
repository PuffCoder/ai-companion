#include "uart_bridge.h"
#include "bsp_board.h"

#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "uart_bridge";

#define RX_BUF_SIZE     1024
#define JSON_LINE_MAX         256    /* 单行 JSON 上限，超长丢弃防溢出 */

static uart_bridge_line_cb_t s_line_cb;

void uart_bridge_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = BSP_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(BSP_UART_PORT, RX_BUF_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(BSP_UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(BSP_UART_PORT,
                                 BSP_UART_TX_GPIO, BSP_UART_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "UART%d ready: TX=%d RX=%d @%d",
             BSP_UART_PORT, BSP_UART_TX_GPIO, BSP_UART_RX_GPIO, BSP_UART_BAUD);
}

void uart_bridge_send_json(const char *json)
{
    if (!json) return;
    uart_write_bytes(BSP_UART_PORT, json, strlen(json));
    uart_write_bytes(BSP_UART_PORT, "\n", 1);
}

/* 逐字节读，按 '\n' 切行，整行回调 */
static void rx_task(void *arg)
{
    char line[JSON_LINE_MAX];
    int  len = 0;
    uint8_t ch;

    while (1) {
        int n = uart_read_bytes(BSP_UART_PORT, &ch, 1, pdMS_TO_TICKS(100));
        if (n != 1) continue;

        if (ch == '\n' || ch == '\r') {
            if (len > 0) {
                line[len] = '\0';
                if (s_line_cb) s_line_cb(line);
                len = 0;
            }
            continue;
        }

        if (len < JSON_LINE_MAX - 1) {
            line[len++] = (char)ch;
        } else {
            ESP_LOGW(TAG, "line overflow, dropped");   /* 丢弃超长行，重新同步 */
            len = 0;
        }
    }
}

void uart_bridge_start_rx(uart_bridge_line_cb_t cb)
{
    s_line_cb = cb;
    /* 优先级 4：低于对话任务(5)，别抢音频/唤醒的 CPU 导致卡顿 */
    xTaskCreate(rx_task, "uart_bridge_rx", 4096, NULL, 4, NULL);
}

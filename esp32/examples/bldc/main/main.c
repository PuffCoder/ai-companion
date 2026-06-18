/*
 * BLDC 第一步：只测 AS5600 磁编码器 —— 不接电机、不接 PD6010B。
 *
 * 接线（见 bsp_board.h）：
 *   AS5600  SDA → GPIO8
 *           SCL → GPIO9
 *           VCC → 3.3V
 *           GND → GND
 *
 * 目的：确认能稳定读到角度，手转磁铁时角度跟着变，
 *       STATUS=MD（检测到磁铁）、AGC 在中间值附近（磁距合适）。
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "as5600.h"

static const char *TAG = "bldc_test";

static void print_magnet_status(uint8_t st)
{
    if (!(st & AS5600_STATUS_MD)) { ESP_LOGW(TAG, "  磁铁未检测到 (MD=0)"); return; }
    if (st & AS5600_STATUS_ML)     ESP_LOGW(TAG, "  磁场太弱 (ML) — 磁铁离太远");
    if (st & AS5600_STATUS_MH)     ESP_LOGW(TAG, "  磁场太强 (MH) — 磁铁离太近");
    if (!(st & (AS5600_STATUS_ML | AS5600_STATUS_MH)))
        ESP_LOGI(TAG, "  磁距正常 (MD)");
}

void app_main(void)
{
    if (as5600_init() != ESP_OK) {
        ESP_LOGE(TAG, "AS5600 初始化失败，停止");
        return;
    }

    /* 上电先看一次磁铁状态 / AGC，确认安装位置 */
    uint8_t st = 0, agc = 0;
    uint16_t mag = 0;
    as5600_read_status(&st);
    as5600_read_agc(&agc);
    as5600_read_magnitude(&mag);
    ESP_LOGI(TAG, "STATUS=0x%02X  AGC=%d (理想~128)  MAGNITUDE=%d", st, agc, mag);
    print_magnet_status(st);

    ESP_LOGI(TAG, "开始读角度，手动转磁铁看数值变化...");
    while (1) {
        uint16_t raw = 0;
        float deg = 0.0f;
        if (as5600_read_raw(&raw) == ESP_OK && as5600_read_deg(&deg) == ESP_OK) {
            ESP_LOGI(TAG, "raw=%4d  angle=%6.2f°", raw, deg);
        } else {
            ESP_LOGE(TAG, "读取失败");
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

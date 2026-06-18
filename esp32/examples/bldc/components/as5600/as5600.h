#pragma once
#include <stdint.h>
#include "esp_err.h"

/*
 * AS5600 12-bit magnetic rotary encoder (I2C, addr 0x36).
 * 接线：SDA→GPIO8  SCL→GPIO9  VCC→3.3V  GND→GND（引脚见 bsp_board.h）
 * 单纯读角度，不驱动电机。
 */

/* STATUS (0x0B) 磁铁检测位 */
#define AS5600_STATUS_MH   (1 << 3)   /* 磁场太强（磁铁太近）*/
#define AS5600_STATUS_ML   (1 << 4)   /* 磁场太弱（磁铁太远）*/
#define AS5600_STATUS_MD   (1 << 5)   /* 检测到磁铁 */

/**
 * @brief 初始化 I2C 总线并探测 AS5600。
 * @return ESP_OK 成功；ESP_ERR_NOT_FOUND 没找到芯片。
 */
esp_err_t as5600_init(void);

/**
 * @brief 读 12-bit 原始角度（RAW ANGLE 0x0C/0x0D），0–4095。
 */
esp_err_t as5600_read_raw(uint16_t *out_raw);

/**
 * @brief 读角度并换算成度（0–360）。
 */
esp_err_t as5600_read_deg(float *out_deg);

/**
 * @brief 读 STATUS（磁铁检测）。配合 AS5600_STATUS_* 位用。
 */
esp_err_t as5600_read_status(uint8_t *out_status);

/**
 * @brief 读 AGC（自动增益 0–255，理想 ~128，越偏说明磁距不合适）。
 */
esp_err_t as5600_read_agc(uint8_t *out_agc);

/**
 * @brief 读 MAGNITUDE（CORDIC 幅值，反映磁场强度）。
 */
esp_err_t as5600_read_magnitude(uint16_t *out_mag);

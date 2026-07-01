#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

/* ======================
 * Display Configuration
 * ====================== */
/* 横屏 landscape：320 宽 x 170 高（竖屏是 170x320，见 MADCTL/offset）*/
#define DISPLAY_WIDTH       320
#define DISPLAY_HEIGHT      170

/* SPI Pins */
#define DISPLAY_PIN_SCLK    12
#define DISPLAY_PIN_MOSI    11
#define DISPLAY_PIN_CS      10
#define DISPLAY_PIN_DC       9
#define DISPLAY_PIN_RST      8
#define DISPLAY_PIN_BL      46

/* SPI Clock Speed */
#define DISPLAY_SPI_FREQ_HZ (40 * 1000 * 1000)  // 40 MHz

/* Common colors (RGB565) */
#define COLOR_BLACK         0x0000
#define COLOR_WHITE         0xFFFF
#define COLOR_RED           0xF800
#define COLOR_GREEN         0x07E0
#define COLOR_BLUE          0x001F
#define COLOR_YELLOW        0xFFE0
#define COLOR_CYAN          0x07FF
#define COLOR_MAGENTA       0xF81F

/* ======================
 * Public API
 * ====================== */

/**
 * @brief Initialize ST7789 display
 */
void display_driver_init(void);

/**
 * @brief Fill entire screen with one color
 */
void display_fill(uint16_t color);

/**
 * @brief Draw a filled rectangle
 */
void display_fill_rect(uint16_t x, uint16_t y,
                       uint16_t w, uint16_t h,
                       uint16_t color);

/**
 * @brief Draw a single pixel
 */
void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief Blit a pre-rendered RGB565 buffer to a region (one SPI transfer, no flicker).
 *        pixels 必须是已字节交换(big-endian)的 RGB565，长度 = w*h。
 */
void display_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels);

/**
 * @brief Set backlight on/off
 */
void display_set_backlight(bool on);

#endif /* DISPLAY_DRIVER_H */

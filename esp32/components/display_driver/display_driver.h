#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "bsp_board.h"

/* ======================
 * Display Configuration
 * ====================== */
#define DISPLAY_WIDTH       BSP_DISPLAY_WIDTH
#define DISPLAY_HEIGHT      BSP_DISPLAY_HEIGHT

/* SPI Pins — defined in bsp_board.h */
#define DISPLAY_PIN_SCLK    BSP_DISPLAY_SCLK
#define DISPLAY_PIN_MOSI    BSP_DISPLAY_MOSI
#define DISPLAY_PIN_CS      BSP_DISPLAY_CS
#define DISPLAY_PIN_DC      BSP_DISPLAY_DC
#define DISPLAY_PIN_RST     BSP_DISPLAY_RST
#define DISPLAY_PIN_BL      BSP_DISPLAY_BL

/* SPI Clock Speed */
#define DISPLAY_SPI_FREQ_HZ (40 * 1000 * 1000)  // 40 MHz

/* Font dimensions (5x7 glyph + 1px spacing) */
#define FONT_CHAR_W  6
#define FONT_CHAR_H  8

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
 * @brief Set backlight on/off
 */
void display_set_backlight(bool on);

/**
 * @brief Draw a single ASCII character (6x8 pixels)
 */
void display_draw_char(uint16_t x, uint16_t y, char c,
                       uint16_t fg, uint16_t bg);

/**
 * @brief Draw a null-terminated ASCII string
 */
void display_draw_string(uint16_t x, uint16_t y, const char *str,
                         uint16_t fg, uint16_t bg);

#endif /* DISPLAY_DRIVER_H */

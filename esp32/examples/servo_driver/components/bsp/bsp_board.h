#ifndef BSP_BOARD_H
#define BSP_BOARD_H

/* ======================
 * Board Selection
 * 取消注释其中一个来选择目标板:
 * ====================== */

#define BSP_TARGET_ESP32_S3
// #define BSP_TARGET_ESP32_C6

/* ======================================================
 * ESP32-S3 Configuration
 * ====================================================== */
#ifdef BSP_TARGET_ESP32_S3

#define BSP_BOARD_NAME        "ESP32-S3"

/* LED */
#define BSP_LED_GPIO          38
#define BSP_LED_COUNT         1

/* Display (ST7789 SPI) */
#define BSP_DISPLAY_SCLK     12
#define BSP_DISPLAY_MOSI     11
#define BSP_DISPLAY_CS       10
#define BSP_DISPLAY_DC        9
#define BSP_DISPLAY_RST       8
#define BSP_DISPLAY_BL       46
#define BSP_DISPLAY_WIDTH   170
#define BSP_DISPLAY_HEIGHT  320

/* Servo (MG90S — PWM via LEDC) */
#define BSP_SERVO_GPIO       14

/* Microphone (INMP441 — I2S input)
 * 注意：GND 和 L/R 用右侧 GND（GPIO19 下方），见 docs/wiring.md */
#define BSP_MIC_I2S_BCLK      1   /* SCK */
#define BSP_MIC_I2S_WS        2   /* WS  */
#define BSP_MIC_I2S_DIN      42   /* SD  */

/* UART (K230 bridge) */
// #define BSP_UART_TX_GPIO   17
// #define BSP_UART_RX_GPIO   18

/* ======================================================
 * ESP32-C6 Configuration
 * ====================================================== */
#elif defined(BSP_TARGET_ESP32_C6)

#define BSP_BOARD_NAME        "ESP32-C6"

/* LED */
#define BSP_LED_GPIO          8
#define BSP_LED_COUNT         1

/* UART */
// #define BSP_UART_TX_GPIO   16
// #define BSP_UART_RX_GPIO   17

#else
#error "No board target defined. Please define BSP_TARGET_ESP32_S3 or BSP_TARGET_ESP32_C6 in bsp_board.h"
#endif

#endif /* BSP_BOARD_H */
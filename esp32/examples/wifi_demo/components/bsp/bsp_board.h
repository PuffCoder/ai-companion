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
#define BSP_LED_GPIO          8
#define BSP_LED_COUNT         1

/* UART */
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
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

/* ======================================================
 * 引脚总览（接线参考单）
 * ------------------------------------------------------
 *  外设            信号        GPIO      供电
 *  屏 ST7789 SPI   SCL/SCLK    4         3.3V
 *                  SDA/MOSI    5
 *                  RES/RST     6
 *                  DC          7
 *                  CS          15
 *                  BLK/BL      16
 *  麦 INMP441 I2S  SCK/BCLK    1         3.3V
 *                  WS          2
 *                  SD/DIN      42
 *                  L/R         21（软件拉低=左声道，代替 GND）
 *  喇叭 MAX98357A  BCLK        41        5V
 *                  LRC         40
 *                  DIN         39
 *  舵机            PWM 信号    14        外部 5V
 *  LED WS2812      DIN         38        5V
 *  UART→K230       TX          17 → K230 pin10 (GPIO4 RX)
 *                  RX          18 ← K230 pin8  (GPIO3 TX)
 *
 *  电源：3.3V(屏/麦)、5V(喇叭/舵机/LED，建议独立电源)、所有 GND 共地(含 K230)
 *  避开：26–37(八线 PSRAM/Flash)、19/20(USB)、43/44(USB控制台)、0/3/45/46(strapping)
 * ====================================================== */

/* LED */
#define BSP_LED_GPIO          38
#define BSP_LED_COUNT         1

/* Display (ST7789 SPI) — 横屏 320x170
 * 屏模块脚序 SCL SDA RES DC CS BLK 顺接 GPIO 4 5 6 7 15 16；VCC=3.3V */
#define BSP_DISPLAY_SCLK      4   /* SCL */
#define BSP_DISPLAY_MOSI      5   /* SDA */
#define BSP_DISPLAY_RST       6   /* RES */
#define BSP_DISPLAY_DC        7   /* DC  */
#define BSP_DISPLAY_CS       15   /* CS  */
#define BSP_DISPLAY_BL       16   /* BLK */
#define BSP_DISPLAY_WIDTH   320
#define BSP_DISPLAY_HEIGHT  170

/* Servo (MG90S — PWM via LEDC)；VCC 用外部 5V */
#define BSP_SERVO_GPIO       14

/* Speaker (MAX98357A — I2S output)；VIN 用 5V */
#define BSP_SPK_I2S_BCLK     41
#define BSP_SPK_I2S_LRC      40
#define BSP_SPK_I2S_DIN      39

/* Microphone (INMP441 — I2S input)
 * L/R 接 GPIO21（软件输出 0 选左声道），省一个 GND 脚；VDD=3.3V */
#define BSP_MIC_I2S_BCLK      1   /* SCK */
#define BSP_MIC_I2S_WS        2   /* WS  */
#define BSP_MIC_I2S_DIN      42   /* SD  */
#define BSP_MIC_LR_GPIO      21   /* L/R → 软件拉低=左声道（代替接 GND）*/

/* UART (K230 bridge) — UART1, 独立于 USB 控制台(UART0)
 * 交叉接线：GPIO17(TX)→K230 pin10(GPIO4 RX)，GPIO18(RX)←K230 pin8(GPIO3 TX)，共 GND。
 * ⚠️ K230 用物理针脚号 pin8/pin10（=GPIO3/4=串口1），不是树莓派标签的 GPIO14/15。
 * 两端都是 3.3V 逻辑，无需电平转换。*/
#define BSP_UART_TX_GPIO     17
#define BSP_UART_RX_GPIO     18
#define BSP_UART_PORT         1
#define BSP_UART_BAUD    115200

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
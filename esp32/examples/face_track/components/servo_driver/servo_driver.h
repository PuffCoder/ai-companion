#pragma once
#include <stdint.h>

/**
 * @brief Initialize MG90S servo on BSP_SERVO_GPIO.
 *        Sets up LEDC 50Hz PWM, parks servo at 90°.
 */
void servo_driver_init(void);

/**
 * @brief Move servo to angle (0–180 degrees).
 */
void servo_set_angle(uint8_t angle);

/**
 * @brief Direct pulse width control (microseconds).
 *        MG90S: 500µs = 0°, 1500µs = 90°, 2500µs = 180°
 */
void servo_set_pulse_us(uint16_t us);

/* ── 连续旋转舵机（速度控制）────────────────── */

/**
 * @brief 连续旋转舵机转速控制。
 * @param speed  -100~+100：0=停，正/负=两个方向，绝对值=快慢
 *               （内部映射脉冲：1500=停，偏离越多越快）
 */
void servo_rotate(int speed);

/**
 * @brief 停止（=servo_rotate(0)）。
 */
void servo_stop(void);

/* ── 标准角度舵机辅助（连续舵机不用）────────── */

/**
 * @brief 读当前角度（软件记录）。
 */
uint8_t servo_get_angle(void);

/**
 * @brief 相对微调：当前角度 +delta（可负），自动限位 0–180。
 *        人脸偏右就 +几度，偏左就 -几度。
 */
void servo_nudge(int delta);

/**
 * @brief 平滑移动到目标角度（逐度过渡，不猛甩）。
 * @param angle    目标角度 0–180
 * @param step_ms  每度停留毫秒（越大越慢，建议 5–15）
 */
void servo_smooth_to(uint8_t angle, uint16_t step_ms);

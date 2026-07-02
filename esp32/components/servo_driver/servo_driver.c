#include "servo_driver.h"
#include "bsp_board.h"

#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "servo_driver";

static uint8_t s_angle = 90;   /* 当前角度（软件记录，用于微调/平滑）*/

/* ── PWM config ──────────────────────────────────────────────── */
#define SERVO_FREQ_HZ       50          // Standard servo frequency
#define SERVO_RESOLUTION    LEDC_TIMER_14_BIT
#define SERVO_PERIOD_US     20000       // 1/50Hz = 20ms
#define SERVO_MIN_US        500         // 0°
#define SERVO_MAX_US        2500        // 180°
#define SERVO_LEDC_TIMER    LEDC_TIMER_1
#define SERVO_LEDC_CHANNEL  LEDC_CHANNEL_1
#define SERVO_LEDC_MODE     LEDC_LOW_SPEED_MODE
#define SERVO_MAX_DUTY      ((1 << 14) - 1)   // 16383

/* ── Helper: microseconds → LEDC duty count ──────────────────── */
static uint32_t us_to_duty(uint16_t us)
{
    return (uint32_t)us * SERVO_MAX_DUTY / SERVO_PERIOD_US;
}

/* ── Public API ──────────────────────────────────────────────── */

void servo_driver_init(void)
{
    ledc_timer_config_t timer = {
        .duty_resolution = SERVO_RESOLUTION,
        .freq_hz         = SERVO_FREQ_HZ,
        .speed_mode      = SERVO_LEDC_MODE,
        .timer_num       = SERVO_LEDC_TIMER,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {
        .channel    = SERVO_LEDC_CHANNEL,
        .duty       = us_to_duty(1500),   // Park at 90°
        .gpio_num   = BSP_SERVO_GPIO,
        .speed_mode = SERVO_LEDC_MODE,
        .timer_sel  = SERVO_LEDC_TIMER,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));

    ESP_LOGI(TAG, "Servo ready on GPIO%d, parked at 90°", BSP_SERVO_GPIO);
}

void servo_set_pulse_us(uint16_t us)
{
    if (us < SERVO_MIN_US) us = SERVO_MIN_US;
    if (us > SERVO_MAX_US) us = SERVO_MAX_US;

    ledc_set_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL, us_to_duty(us));
    ledc_update_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL);
}

void servo_set_angle(uint8_t angle)
{
    if (angle > 180) angle = 180;

    uint16_t us = SERVO_MIN_US +
                  (uint32_t)angle * (SERVO_MAX_US - SERVO_MIN_US) / 180;

    servo_set_pulse_us(us);
    s_angle = angle;
    ESP_LOGD(TAG, "angle=%d° → %dµs", angle, us);
}

/* ── 连续旋转舵机（速度控制）────────────────── */

#define SERVO_STOP_US   1500    /* 停止脉冲（可能要微调校准）*/

void servo_rotate(int speed)
{
    if (speed > 100)  speed = 100;
    if (speed < -100) speed = -100;
    int us = SERVO_STOP_US + speed * 5;     /* ±100 → ±500us */
    servo_set_pulse_us((uint16_t)us);
}

void servo_stop(void)
{
    servo_set_pulse_us(SERVO_STOP_US);
}

/* ── 标准角度舵机辅助（连续舵机不用）────────── */

uint8_t servo_get_angle(void)
{
    return s_angle;
}

void servo_nudge(int delta)
{
    int a = (int)s_angle + delta;
    if (a < 0)   a = 0;
    if (a > 180) a = 180;
    servo_set_angle((uint8_t)a);
}

void servo_smooth_to(uint8_t angle, uint16_t step_ms)
{
    if (angle > 180) angle = 180;
    int dir = (angle > s_angle) ? 1 : -1;
    while (s_angle != angle) {
        servo_set_angle((uint8_t)((int)s_angle + dir));
        vTaskDelay(pdMS_TO_TICKS(step_ms));
    }
}

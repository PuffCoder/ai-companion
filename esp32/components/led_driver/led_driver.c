#include "led_driver.h"

#include "bsp_board.h"

#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"


static const char *TAG = "led_driver";

/* LED strip handle */
static led_strip_handle_t s_led_strip;

/* Initialize LED driver */
void led_driver_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = BSP_LED_GPIO,
        .max_leds = BSP_LED_COUNT,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
    };
        
    ESP_ERROR_CHECK(
        led_strip_new_rmt_device(
            &strip_config,
            &rmt_config,
            &s_led_strip));
    
    ESP_LOGI(TAG, "LED driver initialized");
}

/* Set LED color */
void led_driver_set_rgb(
    uint32_t red,
    uint32_t green,
    uint32_t blue)
{
    led_strip_set_pixel(
        s_led_strip, 
        0, // LED index
        green, 
        red, 
        blue);

    led_strip_refresh(s_led_strip);
}

/* Turn off LED */
void led_driver_off(void)
{
    led_strip_clear(s_led_strip);
}
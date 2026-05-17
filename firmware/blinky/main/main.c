#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led_strip.h"

#define LED_GPIO 8

led_strip_handle_t led_strip;

/* ========================
 LED TASK
======================== */
void led_task(void *pvParameters)
{
    while (1) {
        // RED
        led_strip_set_pixel(led_strip, 0, 25, 110, 10);
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(500));

        // OFF
        led_strip_clear(led_strip);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ========================
 Print TASK
======================== */
void print_task(void *pvParameters)
{

   int cnt = 0;
   while(1) {
        cnt++;
        printf("LED task is running... (Iteration: %d)\n", cnt);
        vTaskDelay(pdMS_TO_TICKS(1000));
   } 
}

void app_main(void)
{
    // led_strip_handle_t led_strip;

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };

    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    
    // Create tasks for red and green LEDs
    xTaskCreate(led_task, "led_task", 2048, NULL, 5, NULL);
    xTaskCreate(print_task, "print_task", 2048, NULL, 5, NULL);

}
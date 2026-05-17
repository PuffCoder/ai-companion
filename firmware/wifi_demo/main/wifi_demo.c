#include "freertos/freeRTOS.h"
#include "freertos/task.h"

#include "wifi_manager.h"
#include "led_driver.h"
#include "http_server.h"

void led_task(void * pvParameter)
{
    while (1) {
        led_driver_set_rgb(10, 0, 71); // Tokyo purple color RGB 
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

        led_driver_off(); // Turn off LED
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    }
}

void app_main(void)
{
    led_driver_init();
    
/*     xTaskCreate(
        led_task,
        "led_task",
        2048,
        NULL,
        5,
        NULL); */


    wifi_manager_init();

    http_server_init();


}
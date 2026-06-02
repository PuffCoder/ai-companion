#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include <stdint.h>

/* Initialize LED driver */
void led_driver_init(void);

/* Set RGB color */
void led_driver_set_rgb(
    uint32_t red, 
    uint32_t green, 
    uint32_t blue);  

/* Turn off LED */
void led_driver_off(void);

#endif
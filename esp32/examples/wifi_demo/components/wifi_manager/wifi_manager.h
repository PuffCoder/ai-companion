#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

/* Initialize and connect to WiFi */
void wifi_manager_init(void);

/* Check WiFi connection status */
bool wifi_manager_is_connected(void);

#endif
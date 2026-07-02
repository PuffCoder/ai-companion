#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

/* Initialize and connect to WiFi */
void wifi_manager_init(void);

/* Check WiFi connection status */
bool wifi_manager_is_connected(void);

/* Get IP address string, e.g. "192.168.1.100". Returns false if not connected. */
bool wifi_manager_get_ip(char *buf, size_t len);

#endif
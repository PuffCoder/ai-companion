#pragma once

/**
 * Initialize UART console REPL and register commands.
 * Call once in app_main. Starts background task automatically.
 *
 * Available commands:
 *   ping <ip>   — ICMP ping (4 packets)
 *   wifi        — Show WiFi status and IP
 *   help        — List commands
 */
void console_init(void);

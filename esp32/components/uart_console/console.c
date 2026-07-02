#include "console.h"
#include "wifi_manager.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "lwip/inet.h"
#include "ping/ping_sock.h"

static const char *TAG = "console";

#define CONSOLE_UART    UART_NUM_0
#define CONSOLE_BUF_LEN 128

/* ── Ping callbacks ──────────────────────────────────────────── */

static void on_ping_success(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    uint8_t  ttl;
    uint32_t elapsed_ms;
    ip_addr_t addr;

    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO,   &seqno,      sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL,     &ttl,        sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_ms, sizeof(elapsed_ms));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR,  &addr,       sizeof(addr));

    printf("Reply from %s: seq=%d ttl=%d time=%ldms\n",
           inet_ntoa(addr.u_addr.ip4), seqno, ttl, elapsed_ms);
}

static void on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    printf("Request timeout: seq=%d\n", seqno);
}

static void on_ping_end(esp_ping_handle_t hdl, void *args)
{
    uint32_t tx, rx, time_ms;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST,  &tx,      sizeof(tx));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY,    &rx,      sizeof(rx));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &time_ms, sizeof(time_ms));

    printf("--- ping statistics ---\n");
    printf("%ld sent, %ld received, %ld%% loss, time %ldms\n\n",
           tx, rx, tx ? (tx - rx) * 100 / tx : 0, time_ms);

    esp_ping_delete_session(hdl);
}

/* ── Command handlers ────────────────────────────────────────── */

static void do_ping(const char *ip_str)
{
    if (!wifi_manager_is_connected()) {
        printf("Error: WiFi not connected\n");
        return;
    }

    ip_addr_t target;
    if (!ipaddr_aton(ip_str, &target)) {
        printf("Error: invalid address '%s'\n", ip_str);
        return;
    }

    printf("PING %s: 32 bytes, 4 packets\n", ip_str);

    esp_ping_config_t cfg  = ESP_PING_DEFAULT_CONFIG();
    cfg.target_addr        = target;
    cfg.count              = 4;
    cfg.interval_ms        = 1000;
    cfg.data_size          = 32;

    esp_ping_callbacks_t cbs = {
        .on_ping_success = on_ping_success,
        .on_ping_timeout = on_ping_timeout,
        .on_ping_end     = on_ping_end,
    };

    esp_ping_handle_t ping;
    esp_ping_new_session(&cfg, &cbs, &ping);
    esp_ping_start(ping);
    /* Results are printed by callbacks; returns immediately */
}

static void do_wifi(void)
{
    if (wifi_manager_is_connected()) {
        char ip[16];
        wifi_manager_get_ip(ip, sizeof(ip));
        printf("Status: Connected\nIP:     %s\n", ip);
    } else {
        printf("Status: Connecting...\n");
    }
}

static void do_help(void)
{
    printf("Commands:\n");
    printf("  ping <ip>  — ICMP ping (4 packets). e.g. ping 8.8.8.8\n");
    printf("  wifi       — Show WiFi status and IP address\n");
    printf("  help       — Show this message\n");
}

static void process_command(char *line)
{
    /* Tokenize: first word = command, rest = args */
    char *cmd = strtok(line, " \t");
    if (!cmd || strlen(cmd) == 0) return;

    if (strcmp(cmd, "ping") == 0) {
        char *ip = strtok(NULL, " \t");
        if (ip) do_ping(ip);
        else    printf("Usage: ping <ip>\n");

    } else if (strcmp(cmd, "wifi") == 0) {
        do_wifi();

    } else if (strcmp(cmd, "help") == 0) {
        do_help();

    } else {
        printf("Unknown command: '%s'. Type 'help'.\n", cmd);
    }
}

/* ── Console task: UART readline with echo ───────────────────── */

static void console_task(void *pv)
{
    char buf[CONSOLE_BUF_LEN];
    int  pos = 0;

    printf("\n\n=== AI Companion Console ===\n");
    printf("Type 'help' for available commands.\n");
    printf("ai> ");
    fflush(stdout);

    while (1) {
        uint8_t c;
        int len = uart_read_bytes(CONSOLE_UART, &c, 1, pdMS_TO_TICKS(20));
        if (len <= 0) continue;

        if (c == '\r' || c == '\n') {
            printf("\n");
            buf[pos] = '\0';
            if (pos > 0) process_command(buf);
            pos = 0;
            printf("ai> ");
            fflush(stdout);

        } else if ((c == '\b' || c == 0x7F) && pos > 0) {
            /* Backspace */
            pos--;
            printf("\b \b");
            fflush(stdout);

        } else if (c >= 0x20 && pos < CONSOLE_BUF_LEN - 1) {
            buf[pos++] = (char)c;
            printf("%c", c);   /* Echo */
            fflush(stdout);
        }
    }
}

/* ── Init ────────────────────────────────────────────────────── */

void console_init(void)
{
    /* Install UART driver on UART0 for byte-level read */
    uart_driver_install(CONSOLE_UART, 256, 0, 0, NULL, 0);

    xTaskCreate(console_task, "console", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Console started on UART0");
}

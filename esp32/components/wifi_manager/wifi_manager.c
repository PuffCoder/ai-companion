#include "wifi_manager.h"

#include <stdio.h>
#include <string.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

static const char *TAG = "wifi_manager";

/* WiFi configuration — set via idf.py menuconfig or sdkconfig.defaults.local */
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASSWORD

/* WiFi connection flag */
static bool s_wifi_connected = false;

/* WiFi event handler */
static void wifi_event_handler(
    void *arg, 
    esp_event_base_t event_base, 
    int32_t event_id, 
    void *event_data)
{
    /* WiFi started */
    if (event_base == WIFI_EVENT && 
        event_id == WIFI_EVENT_STA_START) 
    {
        ESP_LOGI(TAG, "Connecting to WiFi...");
        esp_wifi_connect();
    }

    /* Got IP address */
    else if (event_base == WIFI_EVENT && 
        event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t *event =
            (ip_event_got_ip_t *) event_data;

        s_wifi_connected = true;

        ESP_LOGI(TAG,
                "Connected. IP Address  " IPSTR, 
                IP2STR(&event->ip_info.ip));
    }
    
    /* WiFi disconnected */
    else if (event_base == WIFI_EVENT && 
            event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {
        s_wifi_connected = false;

        ESP_LOGI(TAG, 
                "WiFi disconnected, reconnecting...");

        esp_wifi_connect();
    }
}


/* Initialize WiFi */
void wifi_manager_init(void)
{
    esp_err_t ret;

    /* Initialize NVS */
    ret = nvs_flash_init();

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS");
        return;
    }

    /* Initialize TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Create event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Create default WiFi STA */
    esp_netif_create_default_wifi_sta();

    /* Initialize WiFi */
    wifi_init_config_t cfg = 
        WIFI_INIT_CONFIG_DEFAULT();
    
    /* Initialize WiFi driver */
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Register event handler */
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT, 
            ESP_EVENT_ANY_ID, 
            &wifi_event_handler, 
            NULL,
            NULL));
    
    /* Register IP event handler */
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT, 
            IP_EVENT_STA_GOT_IP, 
            &wifi_event_handler, 
            NULL,
            NULL));

    /* WiFi configuration */
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    /* Set WiFi mode */
    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA, 
            &wifi_config));

    /* Start WiFi */
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi manager initialized.");
}

/* Get WiFi connection status */
bool wifi_manager_is_connected(void)
{
    return s_wifi_connected;
}
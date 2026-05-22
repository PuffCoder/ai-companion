#include "http_server.h"
#include "led_driver.h"

#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "http_server";

/* ============================================
   Root GET handler
 * ============================================ */


static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char *response =
        "Hello from ESP32 HTTP Server!";
    
    httpd_resp_send(
        req,
        response,
        HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}


/* ============================================
   LED ON handler
 * ============================================ */

static esp_err_t led_on_handler(httpd_req_t *req)
{
    led_driver_set_rgb(0, 70, 137); // Set LED to Tokyo Blue color
    const char *response = "LED turned ON";
    
    httpd_resp_send(
        req,
        response,
        HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

/* ============================================
   LED OFF handler
 * ============================================ */
static esp_err_t led_off_handler(httpd_req_t *req)
{
    led_driver_off(); // Turn off LED
    const char *response = "LED turned OFF";
    
    httpd_resp_send(
        req,
        response,
        HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

/* ============================================
   URI Handler
 * ============================================ */

static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler,
    .user_ctx = NULL
};

/* ============================================
   LED ON URI Handler
 * ============================================ */
static const httpd_uri_t led_on = {
    .uri = "/led/on",
    .method = HTTP_GET,
    .handler = led_on_handler,
    .user_ctx = NULL
};

/* ============================================
   LED OFF URI Handler
 * ============================================ */
static const httpd_uri_t led_off = {
    .uri = "/led/off",
    .method = HTTP_GET,
    .handler = led_off_handler,
    .user_ctx = NULL
};


/* ============================================
   Start HTTP Server
 * ============================================ */

void http_server_init(void)
{
    httpd_handle_t server = NULL;

    httpd_config_t config = 
        HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(TAG, 
            "Starting HTTP Server on port: %d", config.server_port);

    if (httpd_start(&server, &config) 
        == ESP_OK) 
    {
        /* Register URI handlers   */
        httpd_register_uri_handler(
            server, 
            &root);
        httpd_register_uri_handler(
            server, 
            &led_on);
        httpd_register_uri_handler(
            server, 
            &led_off);

        ESP_LOGI(TAG, 
                "HTTP Server started successfully");

    } 
    else 
    {
        ESP_LOGE(TAG, 
                "Failed to start HTTP Server");
    }
}
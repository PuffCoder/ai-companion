#include "cloud_api.h"

#include <string.h>
#include <stdio.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "cloud_api";

#define CLAUDE_API_URL     "https://api.anthropic.com/v1/messages"
#define CLAUDE_MODEL       "claude-haiku-4-5-20251001"
#define CLAUDE_MAX_TOKENS  256
#define CLAUDE_API_VERSION "2023-06-01"

/* ── Pure functions (testable on host) ───────────────────────── */

int cloud_api_build_request(const char *user_message, char *out_buf, int buf_len)
{
    cJSON *root     = cJSON_CreateObject();
    cJSON *messages = cJSON_CreateArray();
    cJSON *msg      = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "model", CLAUDE_MODEL);
    cJSON_AddNumberToObject(root, "max_tokens", CLAUDE_MAX_TOKENS);

    cJSON_AddStringToObject(msg, "role", "user");
    cJSON_AddStringToObject(msg, "content", user_message);
    cJSON_AddItemToArray(messages, msg);
    cJSON_AddItemToObject(root, "messages", messages);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str) return -1;

    int len = strlen(json_str);
    if (len >= buf_len) {
        free(json_str);
        return -1;
    }

    memcpy(out_buf, json_str, len + 1);
    free(json_str);
    return len;
}

bool cloud_api_parse_response(const char *json, char *out_text, int out_len)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;

    cJSON *content = cJSON_GetObjectItem(root, "content");
    if (!cJSON_IsArray(content) || cJSON_GetArraySize(content) == 0) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *first = cJSON_GetArrayItem(content, 0);
    cJSON *text  = cJSON_GetObjectItem(first, "text");
    if (!cJSON_IsString(text)) {
        cJSON_Delete(root);
        return false;
    }

    strncpy(out_text, text->valuestring, out_len - 1);
    out_text[out_len - 1] = '\0';

    cJSON_Delete(root);
    return true;
}

/* ── Hardware-dependent functions ────────────────────────────── */

static char s_response_buf[CLOUD_API_MAX_RESPONSE_LEN];
static int  s_response_len = 0;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        int remaining = sizeof(s_response_buf) - s_response_len - 1;
        int to_copy   = evt->data_len < remaining ? evt->data_len : remaining;
        memcpy(s_response_buf + s_response_len, evt->data, to_copy);
        s_response_len += to_copy;
    }
    return ESP_OK;
}

void cloud_api_init(void)
{
    ESP_LOGI(TAG, "Cloud API initialized (Claude %s)", CLAUDE_MODEL);
}

cloud_api_result_t cloud_api_chat(const char *user_message)
{
    cloud_api_result_t result = {.success = false};

    char request_body[CLOUD_API_MAX_PROMPT_LEN + 256];
    if (cloud_api_build_request(user_message, request_body, sizeof(request_body)) < 0) {
        ESP_LOGE(TAG, "Failed to build request");
        return result;
    }

    s_response_len = 0;
    memset(s_response_buf, 0, sizeof(s_response_buf));

    esp_http_client_config_t config = {
        .url            = CLAUDE_API_URL,
        .event_handler  = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type",      "application/json");
    esp_http_client_set_header(client, "x-api-key",         CONFIG_CLAUDE_API_KEY);
    esp_http_client_set_header(client, "anthropic-version", CLAUDE_API_VERSION);
    esp_http_client_set_post_field(client, request_body, strlen(request_body));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status == 200) {
            result.success = cloud_api_parse_response(s_response_buf,
                                                      result.response,
                                                      sizeof(result.response));
        } else {
            ESP_LOGE(TAG, "HTTP %d: %s", status, s_response_buf);
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return result;
}

#include "llm.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"

static const char *TAG = "llm";

#define LLM_URL       "https://api.openai.com/v1/chat/completions"
#define LLM_MODEL     "gpt-4o-mini"
#define MAX_MESSAGES  11      /* system + 5 轮（user+assistant），超了丢最旧 */
#define RESP_MAX      4096

static cJSON *s_messages;     /* {role, content} 数组 */
static char   s_system[256];

void llm_init(const char *system_prompt)
{
    if (s_messages) cJSON_Delete(s_messages);
    s_messages = cJSON_CreateArray();

    snprintf(s_system, sizeof(s_system), "%s",
        system_prompt ? system_prompt
                      : "你是AI机器人小黄人，性格活泼可爱，回答简短（30字内）。");

    cJSON *sys = cJSON_CreateObject();
    cJSON_AddStringToObject(sys, "role", "system");
    cJSON_AddStringToObject(sys, "content", s_system);
    cJSON_AddItemToArray(s_messages, sys);
}

void llm_reset(void)
{
    llm_init(s_system);
}

/* 历史过长则丢最旧的非 system 消息 */
static void trim_history(void)
{
    while (cJSON_GetArraySize(s_messages) > MAX_MESSAGES) {
        cJSON_DeleteItemFromArray(s_messages, 1);   /* index 0 是 system，保留 */
    }
}

/* HTTP 响应收集 */
static char *s_resp;
static int   s_resp_len;

static esp_err_t http_evt(esp_http_client_event_t *e)
{
    if (e->event_id == HTTP_EVENT_ON_DATA && s_resp_len < RESP_MAX - 1) {
        int copy = e->data_len;
        if (s_resp_len + copy > RESP_MAX - 1) copy = RESP_MAX - 1 - s_resp_len;
        memcpy(s_resp + s_resp_len, e->data, copy);
        s_resp_len += copy;
    }
    return ESP_OK;
}

bool llm_chat(const char *api_key, const char *user_msg,
              char *out, int out_len)
{
    bool ok = false;
    if (!s_messages) llm_init(NULL);

    /* 把用户消息加入历史 */
    cJSON *um = cJSON_CreateObject();
    cJSON_AddStringToObject(um, "role", "user");
    cJSON_AddStringToObject(um, "content", user_msg);
    cJSON_AddItemToArray(s_messages, um);
    trim_history();

    /* 构建请求体 */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", LLM_MODEL);
    cJSON_AddNumberToObject(root, "max_tokens", 200);
    cJSON_AddItemToObject(root, "messages", cJSON_Duplicate(s_messages, 1));
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return false;

    s_resp = heap_caps_malloc(RESP_MAX, MALLOC_CAP_SPIRAM);
    if (!s_resp) { free(body); return false; }
    s_resp_len = 0;

    char auth[256];
    snprintf(auth, sizeof(auth), "Bearer %s", api_key);

    esp_http_client_config_t cfg = {
        .url               = LLM_URL,
        .method            = HTTP_METHOD_POST,
        .event_handler     = http_evt,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = 20000,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    esp_http_client_set_header(c, "Authorization", auth);
    esp_http_client_set_header(c, "Content-Type", "application/json");
    esp_http_client_set_post_field(c, body, strlen(body));

    esp_err_t err = esp_http_client_perform(c);
    int st = esp_http_client_get_status_code(c);

    if (err == ESP_OK && st == 200) {
        s_resp[s_resp_len] = 0;
        cJSON *j = cJSON_Parse(s_resp);
        cJSON *choices = j ? cJSON_GetObjectItem(j, "choices") : NULL;
        cJSON *c0 = choices ? cJSON_GetArrayItem(choices, 0) : NULL;
        cJSON *msg = c0 ? cJSON_GetObjectItem(c0, "message") : NULL;
        cJSON *content = msg ? cJSON_GetObjectItem(msg, "content") : NULL;
        if (cJSON_IsString(content)) {
            strncpy(out, content->valuestring, out_len - 1);
            out[out_len - 1] = 0;
            /* 回复加入历史 */
            cJSON *am = cJSON_CreateObject();
            cJSON_AddStringToObject(am, "role", "assistant");
            cJSON_AddStringToObject(am, "content", out);
            cJSON_AddItemToArray(s_messages, am);
            ok = true;
        }
        cJSON_Delete(j);
    } else {
        s_resp[s_resp_len < RESP_MAX - 1 ? s_resp_len : RESP_MAX - 1] = 0;
        ESP_LOGE(TAG, "chat 失败 err=%d http=%d", err, st);
    }

    esp_http_client_cleanup(c);
    free(body);
    heap_caps_free(s_resp);
    return ok;
}

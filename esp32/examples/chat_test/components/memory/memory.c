#include "memory.h"

#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "memory";

#define NVS_NS      "ai_mem"      /* NVS 命名空间 */
#define KEY_USER    "user_info"   /* 用户信息 key */
#define KEY_PERSONA "persona"     /* 人设 key */

void memory_init(void)
{
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
}

void memory_set_user(const char *info)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGE(TAG, "NVS 打开失败");
        return;
    }
    nvs_set_str(h, KEY_USER, info);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "用户信息已存入 flash");
}

bool memory_get_user(char *out, int out_len)
{
    nvs_handle_t h;
    out[0] = '\0';
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        return false;   /* 还没存过 */
    }
    size_t len = out_len;
    esp_err_t r = nvs_get_str(h, KEY_USER, out, &len);
    nvs_close(h);
    return (r == ESP_OK && out[0] != '\0');
}

void memory_append_user(const char *fact)
{
    char cur[USER_MEMORY_MAX];
    if (!memory_get_user(cur, sizeof(cur))) cur[0] = '\0';

    if (strstr(cur, fact)) {            /* 已存在，去重跳过 */
        ESP_LOGI(TAG, "记忆已有，跳过");
        return;
    }

    size_t len = strlen(cur);
    if (len + strlen(fact) + 2 >= USER_MEMORY_MAX) {
        ESP_LOGW(TAG, "记忆已满，跳过新事实");
        return;
    }
    if (len > 0) strncat(cur, "；", sizeof(cur) - len - 1);
    strncat(cur, fact, sizeof(cur) - strlen(cur) - 1);
    memory_set_user(cur);
    ESP_LOGI(TAG, "记忆已更新");
}

void memory_set_persona(const char *persona)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGE(TAG, "NVS 打开失败");
        return;
    }
    nvs_set_str(h, KEY_PERSONA, persona);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "人设已存入 flash");
}

bool memory_get_persona(char *out, int out_len)
{
    nvs_handle_t h;
    out[0] = '\0';
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    size_t len = out_len;
    esp_err_t r = nvs_get_str(h, KEY_PERSONA, out, &len);
    nvs_close(h);
    return (r == ESP_OK && out[0] != '\0');
}

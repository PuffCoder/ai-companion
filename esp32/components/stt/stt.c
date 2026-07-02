#include "stt.h"

#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"

static const char *TAG = "stt";

#define WHISPER_URL  "https://api.openai.com/v1/audio/transcriptions"
#define RESP_MAX     8192

/* 写 44 字节 WAV 头（16 位单声道）*/
static void wav_header(uint8_t *h, int data_len, int rate)
{
    int   br = rate * 2;
    short fmt = 1, ch = 1, ba = 2, bps = 16;
    int   sub1 = 16, chunk = 36 + data_len;
    memcpy(h,      "RIFF", 4); memcpy(h + 4,  &chunk, 4);
    memcpy(h + 8,  "WAVE", 4); memcpy(h + 12, "fmt ", 4);
    memcpy(h + 16, &sub1, 4);  memcpy(h + 20, &fmt, 2);
    memcpy(h + 22, &ch, 2);    memcpy(h + 24, &rate, 4);
    memcpy(h + 28, &br, 4);    memcpy(h + 32, &ba, 2);
    memcpy(h + 34, &bps, 2);   memcpy(h + 36, "data", 4);
    memcpy(h + 40, &data_len, 4);
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

bool stt_transcribe_pcm(const int16_t *pcm, int samples, int sample_rate,
                        const char *api_key, const char *lang,
                        char *out_text, int out_len)
{
    bool ok = false;
    int  data_len = samples * 2;
    int  wav_len  = 44 + data_len;

    /* 组装 multipart 各段 */
    const char *bd = "----esp32boundary";
    char p1[200], p2[200], p3[200], foot[64];
    int l1 = snprintf(p1, sizeof(p1),
        "--%s\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-1\r\n", bd);
    int l2 = 0;
    if (lang) l2 = snprintf(p2, sizeof(p2),
        "--%s\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\n%s\r\n", bd, lang);
    int l3 = snprintf(p3, sizeof(p3),
        "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"rec.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n", bd);
    int lf = snprintf(foot, sizeof(foot), "\r\n--%s--\r\n", bd);

    int total = l1 + l2 + l3 + wav_len + lf;
    uint8_t *body = heap_caps_malloc(total, MALLOC_CAP_SPIRAM);
    s_resp = heap_caps_malloc(RESP_MAX, MALLOC_CAP_SPIRAM);
    if (!body || !s_resp) {
        ESP_LOGE(TAG, "PSRAM 分配失败");
        goto cleanup;
    }
    s_resp_len = 0;

    /* 拼 body：段头 + WAV头 + PCM + 结尾 */
    int off = 0;
    memcpy(body + off, p1, l1); off += l1;
    if (lang) { memcpy(body + off, p2, l2); off += l2; }
    memcpy(body + off, p3, l3); off += l3;
    wav_header(body + off, data_len, sample_rate); off += 44;
    memcpy(body + off, pcm, data_len); off += data_len;
    memcpy(body + off, foot, lf); off += lf;

    char ct[128], auth[256];
    snprintf(ct,   sizeof(ct),   "multipart/form-data; boundary=%s", bd);
    snprintf(auth, sizeof(auth), "Bearer %s", api_key);

    esp_http_client_config_t cfg = {
        .url               = WHISPER_URL,
        .method            = HTTP_METHOD_POST,
        .event_handler     = http_evt,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = 20000,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    esp_http_client_set_header(c, "Authorization", auth);
    esp_http_client_set_header(c, "Content-Type", ct);
    esp_http_client_set_post_field(c, (const char *)body, total);

    ESP_LOGI(TAG, "上传 Whisper (%d bytes)...", total);
    esp_err_t err = esp_http_client_perform(c);

    if (err == ESP_OK) {
        int st = esp_http_client_get_status_code(c);
        s_resp[s_resp_len] = 0;
        if (st == 200) {
            cJSON *j = cJSON_Parse(s_resp);
            cJSON *t = j ? cJSON_GetObjectItem(j, "text") : NULL;
            if (cJSON_IsString(t)) {
                strncpy(out_text, t->valuestring, out_len - 1);
                out_text[out_len - 1] = 0;
                ok = true;
            } else {
                ESP_LOGW(TAG, "无 text 字段");
            }
            cJSON_Delete(j);
        } else {
            ESP_LOGE(TAG, "HTTP %d", st);
        }
    } else {
        ESP_LOGE(TAG, "请求失败: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(c);

cleanup:
    if (body)   heap_caps_free(body);
    if (s_resp) heap_caps_free(s_resp);
    return ok;
}

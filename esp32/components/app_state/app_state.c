#include "app_state.h"
#include "esp_log.h"

static const char *TAG = "app_state";

static device_state_t    s_state;
static EventGroupHandle_t s_events;

static const char *state_name(device_state_t s)
{
    switch (s) {
        case STATE_IDLE:       return "IDLE";
        case STATE_LISTENING:  return "LISTENING";
        case STATE_PROCESSING: return "PROCESSING";
        case STATE_SPEAKING:   return "SPEAKING";
        default:               return "UNKNOWN";
    }
}

void app_state_init(void)
{
    s_state  = STATE_IDLE;
    s_events = xEventGroupCreate();
    ESP_LOGI(TAG, "State machine initialized → %s", state_name(s_state));
}

device_state_t app_state_get(void)
{
    return s_state;
}

void app_state_set(device_state_t state)
{
    if (state == s_state) return;   /* 状态没变，不打印不做事 */
    ESP_LOGI(TAG, "%s → %s", state_name(s_state), state_name(state));
    s_state = state;
}

EventGroupHandle_t app_state_events(void)
{
    return s_events;
}

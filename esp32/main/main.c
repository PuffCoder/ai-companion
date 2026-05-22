#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "app_state.h"
#include "led_driver.h"
#include "wifi_manager.h"
#include "cloud_api.h"

/* ── Main task: state machine loop ──────────────────────────── */

static void main_task(void *pv)
{
    EventGroupHandle_t events = app_state_events();

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(
            events, ALL_EVENTS, pdTRUE, pdFALSE, portMAX_DELAY);

        if (bits & EVENT_ERROR) {
            app_state_set(STATE_IDLE);
            continue;
        }

        switch (app_state_get()) {

            case STATE_IDLE:
                if (bits & EVENT_WAKE_WORD) {
                    app_state_set(STATE_LISTENING);
                }
                break;

            case STATE_LISTENING:
                if (bits & EVENT_AUDIO_READY) {
                    app_state_set(STATE_PROCESSING);
                    /* TODO: cloud_api_chat() */
                }
                break;

            case STATE_PROCESSING:
                if (bits & EVENT_RESPONSE_RDY) {
                    app_state_set(STATE_SPEAKING);
                    /* TODO: play TTS audio */
                }
                break;

            case STATE_SPEAKING:
                if (bits & EVENT_PLAYBACK_END) {
                    app_state_set(STATE_IDLE);
                }
                break;
        }
    }
}

/* ── Temporary: simulate wake word via button / serial ──────── */

static void simulate_task(void *pv)
{
    vTaskDelay(pdMS_TO_TICKS(3000)); // 等 WiFi 连上

    while (1) {
        // 每 5 秒模拟一次唤醒词，用于在麦克风接好前测试流程
        vTaskDelay(pdMS_TO_TICKS(5000));
        xEventGroupSetBits(app_state_events(), EVENT_WAKE_WORD);

        vTaskDelay(pdMS_TO_TICKS(2000));
        xEventGroupSetBits(app_state_events(), EVENT_AUDIO_READY);

        vTaskDelay(pdMS_TO_TICKS(2000));
        xEventGroupSetBits(app_state_events(), EVENT_RESPONSE_RDY);

        vTaskDelay(pdMS_TO_TICKS(2000));
        xEventGroupSetBits(app_state_events(), EVENT_PLAYBACK_END);
    }
}

/* ── Entry point ─────────────────────────────────────────────── */

void app_main(void)
{
    led_driver_init();
    app_state_init();
    wifi_manager_init();
    cloud_api_init();

    xTaskCreate(main_task,     "main_task",     4096, NULL, 5, NULL);
    xTaskCreate(simulate_task, "simulate_task", 2048, NULL, 4, NULL);
}

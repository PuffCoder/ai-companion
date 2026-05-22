#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

/* ── States ─────────────────────────────────────────────────── */

typedef enum {
    STATE_IDLE,        // 待机，等待唤醒
    STATE_LISTENING,   // 正在录音
    STATE_PROCESSING,  // 发送给 Claude，等待回复
    STATE_SPEAKING,    // 正在播放 TTS
} device_state_t;

/* ── Events (bits in EventGroup) ────────────────────────────── */

#define EVENT_WAKE_WORD    (1 << 0)  // 检测到唤醒词
#define EVENT_AUDIO_READY  (1 << 1)  // 录音完成，可以发送
#define EVENT_RESPONSE_RDY (1 << 2)  // Claude 回复就绪
#define EVENT_PLAYBACK_END (1 << 3)  // 播放结束
#define EVENT_ERROR        (1 << 4)  // 任意错误，回到 IDLE

#define ALL_EVENTS (EVENT_WAKE_WORD | EVENT_AUDIO_READY | \
                    EVENT_RESPONSE_RDY | EVENT_PLAYBACK_END | EVENT_ERROR)

/* ── API ─────────────────────────────────────────────────────── */

void             app_state_init(void);
device_state_t   app_state_get(void);
void             app_state_set(device_state_t state);
EventGroupHandle_t app_state_events(void);

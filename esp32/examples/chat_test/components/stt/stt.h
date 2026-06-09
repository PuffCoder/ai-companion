#pragma once

#include <stdint.h>
#include <stdbool.h>

/* 语音转文字（OpenAI Whisper）。
 * 需要：已连 WiFi；证书 bundle（完整 + cross-signed 验证）；PSRAM。 */

/**
 * @brief 把 16 位 PCM 音频转成文字。
 * @param pcm         16 位单声道 PCM 采样
 * @param samples     采样数
 * @param sample_rate 采样率（如 16000）
 * @param api_key     OpenAI API Key（由调用者从 config 传入，组件不存密钥）
 * @param lang        语言代码（如 "zh"；NULL 表示自动检测）
 * @param out_text    输出文字缓冲
 * @param out_len     out_text 容量
 * @return 成功返回 true
 */
bool stt_transcribe_pcm(const int16_t *pcm, int samples, int sample_rate,
                        const char *api_key, const char *lang,
                        char *out_text, int out_len);

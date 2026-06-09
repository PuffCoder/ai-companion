#pragma once

#include <stdbool.h>

/* GPT 对话（OpenAI Chat Completions）。
 * 需要：已连 WiFi；证书 bundle（完整 + cross-signed）；PSRAM。
 * 自带多轮对话历史。 */

/**
 * @brief 初始化对话（设置人设，清空历史）。
 * @param system_prompt 人设/系统提示（NULL 用默认）
 */
void llm_init(const char *system_prompt);

/**
 * @brief 发一条用户消息，返回 AI 回复。自动维护多轮历史。
 * @param api_key  OpenAI API Key（调用者从 config 传入，组件不存密钥）
 * @param user_msg 用户消息
 * @param out      回复输出缓冲
 * @param out_len  out 容量
 * @return 成功返回 true
 */
bool llm_chat(const char *api_key, const char *user_msg,
              char *out, int out_len);

/**
 * @brief 清空对话历史（开启新话题）。
 */
void llm_reset(void);

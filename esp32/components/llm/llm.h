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

/* 流式回调：每收到一小段文字回调一次（delta 是增量片段）*/
typedef void (*llm_stream_cb_t)(const char *delta, void *arg);

/**
 * @brief 流式对话：边生成边通过 on_delta 回调吐字，完整回复写入 out。
 * @param api_key   OpenAI API Key
 * @param user_msg  用户消息
 * @param on_delta  每块增量文字的回调（可为 NULL）
 * @param arg       传给回调的用户参数
 * @param out       完整回复输出缓冲
 * @param out_len   out 容量
 * @return 成功返回 true
 */
bool llm_chat_stream(const char *api_key, const char *user_msg,
                     llm_stream_cb_t on_delta, void *arg,
                     char *out, int out_len);

/**
 * @brief 更新系统提示（人设/记忆变化时调用），保留对话历史。
 */
void llm_set_system(const char *system_prompt);

/**
 * @brief 清空对话历史（开启新话题）。
 */
void llm_reset(void);

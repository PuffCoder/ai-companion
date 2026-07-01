#pragma once

#include <stdbool.h>

/* 长期记忆：把用户信息存进 NVS（flash），断电/重启不丢。
 * 只存「事实摘要」（几百字），覆盖更新，不会膨胀。 */

#define USER_MEMORY_MAX 512   /* 用户信息上限（字节）*/
#define PERSONA_MAX     512   /* 人设上限（字节）*/

/* ── 人设模板库（可扩展）────────────────────── */
#define PERSONA_MINION  "You are a Minion: playful and mischievous, love bananas, talk in short cute sentences, occasionally silly. Always reply in English."
#define PERSONA_BUTLER  "You are an elegant butler: polite, composed, courteous and thoughtful. Always reply in English."
#define PERSONA_FRIEND  "You are a close friend: relaxed, funny, joking around like old buddies. Always reply in English."

/* 默认人设（首次开机写入 NVS）*/
#define PERSONA_DEFAULT PERSONA_MINION

/**
 * @brief 初始化（NVS）。app_main 里调一次。
 */
void memory_init(void);

/**
 * @brief 写入用户信息（覆盖，存 flash）。
 */
void memory_set_user(const char *info);

/**
 * @brief 读出用户信息。没有则 out 为空字符串。
 * @return 有内容返回 true
 */
bool memory_get_user(char *out, int out_len);

/**
 * @brief 追加一条事实到用户信息（合并存 flash，有上限保护）。
 */
void memory_append_user(const char *fact);

/**
 * @brief 写入人设（覆盖，存 flash）。
 */
void memory_set_persona(const char *persona);

/**
 * @brief 读出人设。没存过则返回 false。
 */
bool memory_get_persona(char *out, int out_len);

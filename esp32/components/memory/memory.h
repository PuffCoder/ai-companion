#pragma once

#include <stdbool.h>

/* Long-term memory: stores user info in NVS (flash), survives power loss/reboot.
 * Keeps only a short fact summary (a few hundred bytes), overwritten in place. */

#define USER_MEMORY_MAX 512   /* max user-info bytes */
#define PERSONA_MAX     512   /* max persona bytes */

/* ── Persona templates (edit here to change personality) ────────
 * To change the robot's character, edit these prompts (or add a new one).
 * Note: persona is stored in NVS — after editing, run `idf.py erase-flash`
 * for it to take effect (see below). */
#define PERSONA_MINION  "You are a Minion: playful and mischievous, love bananas, talk in short cute sentences, occasionally silly. Always reply in English."
#define PERSONA_BUTLER  "You are an elegant butler: polite, composed, courteous and thoughtful. Always reply in English."
#define PERSONA_FRIEND  "You are a close friend: relaxed, funny, joking around like old buddies. Always reply in English."

/* Default persona (written to NVS on first boot) — change this line to switch */
#define PERSONA_DEFAULT PERSONA_FRIEND

/**
 * @brief Init (NVS). Call once in app_main.
 */
void memory_init(void);

/**
 * @brief Write user info (overwrite, stored in flash).
 */
void memory_set_user(const char *info);

/**
 * @brief Read user info. out is empty string if none.
 * @return true if there is content
 */
bool memory_get_user(char *out, int out_len);

/**
 * @brief Append a fact to user info (merged into flash, capped).
 */
void memory_append_user(const char *fact);

/**
 * @brief Write persona (overwrite, stored in flash).
 */
void memory_set_persona(const char *persona);

/**
 * @brief Read persona. Returns false if never stored.
 */
bool memory_get_persona(char *out, int out_len);

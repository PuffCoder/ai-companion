#pragma once

#include <stdbool.h>

#define CLOUD_API_MAX_RESPONSE_LEN 1024
#define CLOUD_API_MAX_PROMPT_LEN   512

typedef struct {
    char response[CLOUD_API_MAX_RESPONSE_LEN];
    bool success;
} cloud_api_result_t;

/**
 * Initialize cloud API (sets up HTTP client, loads API key from config).
 */
void cloud_api_init(void);

/**
 * Send a message to Claude and get a response.
 * Blocks until response received or timeout.
 */
cloud_api_result_t cloud_api_chat(const char *user_message);

/**
 * Build Claude API JSON request body into `out_buf`.
 * Returns number of bytes written, or -1 on error.
 * Pure function — testable on host.
 */
int cloud_api_build_request(const char *user_message, char *out_buf, int buf_len);

/**
 * Parse Claude API JSON response, extract text content into `out_text`.
 * Returns true on success.
 * Pure function — testable on host.
 */
bool cloud_api_parse_response(const char *json, char *out_text, int out_len);

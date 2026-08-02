#include "unity.h"
#include "cloud_api.h"
#include <string.h>

/* ── build_request tests ─────────────────────────────────────── */

void test_build_request_contains_model(void)
{
    char buf[512];
    int len = cloud_api_build_request("hello", buf, sizeof(buf));

    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "claude-haiku"));
}

void test_build_request_contains_user_message(void)
{
    char buf[512];
    cloud_api_build_request("hello world", buf, sizeof(buf));

    TEST_ASSERT_NOT_NULL(strstr(buf, "hello world"));
}

void test_build_request_buffer_too_small_returns_error(void)
{
    char small_buf[8];
    int len = cloud_api_build_request("hello", small_buf, sizeof(small_buf));

    TEST_ASSERT_EQUAL(-1, len);
}

/* ── parse_response tests ────────────────────────────────────── */

void test_parse_response_extracts_text(void)
{
    const char *json =
        "{\"content\":[{\"type\":\"text\",\"text\":\"hello!\"}]}";

    char out[128];
    bool ok = cloud_api_parse_response(json, out, sizeof(out));

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello!", out);
}

void test_parse_response_invalid_json_returns_false(void)
{
    char out[128];
    bool ok = cloud_api_parse_response("not json", out, sizeof(out));

    TEST_ASSERT_FALSE(ok);
}

/* ── main ────────────────────────────────────────────────────── */

void app_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_build_request_contains_model);
    RUN_TEST(test_build_request_contains_user_message);
    RUN_TEST(test_build_request_buffer_too_small_returns_error);
    RUN_TEST(test_parse_response_extracts_text);
    RUN_TEST(test_parse_response_invalid_json_returns_false);
    UNITY_END();
}

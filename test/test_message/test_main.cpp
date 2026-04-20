#include <unity.h>
#include "pure/message_sanitize.h"

void setUp() {}
void tearDown() {}

void test_ascii_passthrough() {
    auto out = pure::sanitizeMessage("Hello, world! 123 ~");
    TEST_ASSERT_EQUAL_STRING("Hello, world! 123 ~", out.c_str());
}

void test_strips_emoji_and_high_bytes() {
    // "Hi" + 4-byte UTF-8 emoji "😀" (F0 9F 98 80) + "!"
    const char input[] = { 'H', 'i', (char)0xF0, (char)0x9F, (char)0x98, (char)0x80, '!', 0 };
    auto out = pure::sanitizeMessage(input);
    TEST_ASSERT_EQUAL_STRING("Hi!", out.c_str());
}

void test_preserves_newlines_and_carriage_returns() {
    auto out = pure::sanitizeMessage("line1\nline2\r\nline3");
    TEST_ASSERT_EQUAL_STRING("line1\nline2\r\nline3", out.c_str());
}

void test_strips_other_control_chars() {
    // \t (0x09), \b (0x08), bell (0x07) should all be stripped.
    // Note the string-concat to terminate the \x07 hex escape — otherwise
    // "\x07d" is parsed as 0x7D ('}').
    auto out = pure::sanitizeMessage("a\tb\bc\x07" "d");
    TEST_ASSERT_EQUAL_STRING("abcd", out.c_str());
}

void test_empty_input() {
    auto out = pure::sanitizeMessage("");
    TEST_ASSERT_EQUAL_STRING("", out.c_str());
}

void test_null_input_safe() {
    auto out = pure::sanitizeMessage(nullptr);
    TEST_ASSERT_EQUAL_STRING("", out.c_str());
}

void test_only_emoji_becomes_empty() {
    const char input[] = { (char)0xF0, (char)0x9F, (char)0x98, (char)0x80, 0 };
    auto out = pure::sanitizeMessage(input);
    TEST_ASSERT_EQUAL_STRING("", out.c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ascii_passthrough);
    RUN_TEST(test_strips_emoji_and_high_bytes);
    RUN_TEST(test_preserves_newlines_and_carriage_returns);
    RUN_TEST(test_strips_other_control_chars);
    RUN_TEST(test_empty_input);
    RUN_TEST(test_null_input_safe);
    RUN_TEST(test_only_emoji_becomes_empty);
    return UNITY_END();
}

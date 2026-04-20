#include <unity.h>
#include "pure/keyboard_logic.h"

void setUp() {}
void tearDown() {}

// Layout (from src/pure/keyboard_logic.h):
//   row 0: !@#$%^&*()  origin (35, 200)
//   row 1: 1234567890  origin (35, 245)
//   row 2: QWERTYUIOP  origin (35, 290)
//   row 3: ASDFGHJKL   origin (35, 335)
//   row 4: ZXCVBNM     origin (115, 380)
// keyW = 68, rowH = 40.

static int center(int origin, int keyIdx) { return origin + keyIdx * 68 + 34; }
static int rowMid(int rowY) { return rowY + 20; }

void test_every_key_in_every_row_caps_off() {
    static const char* expectedRows[] = {
        "!@#$%^&*()",
        "1234567890",
        "qwertyuiop",
        "asdfghjkl",
        "zxcvbnm",
    };
    static const int rowYs[]  = { 200, 245, 290, 335, 380 };
    static const int rowXs[]  = { 35, 35, 35, 70, 115 };

    for (int r = 0; r < 5; ++r) {
        int len = 0;
        while (expectedRows[r][len]) ++len;
        for (int k = 0; k < len; ++k) {
            char got = pure::keyboardCharAt(center(rowXs[r], k), rowMid(rowYs[r]), false);
            TEST_ASSERT_EQUAL_CHAR(expectedRows[r][k], got);
        }
    }
}

void test_letters_uppercased_when_caps_on() {
    TEST_ASSERT_EQUAL_CHAR('Q', pure::keyboardCharAt(center(35, 0), rowMid(290), true));
    TEST_ASSERT_EQUAL_CHAR('M', pure::keyboardCharAt(center(115, 6), rowMid(380), true));
}

void test_symbols_unaffected_by_caps() {
    TEST_ASSERT_EQUAL_CHAR('!', pure::keyboardCharAt(center(35, 0), rowMid(200), true));
    TEST_ASSERT_EQUAL_CHAR('5', pure::keyboardCharAt(center(35, 4), rowMid(245), true));
}

void test_out_of_bounds_returns_zero() {
    TEST_ASSERT_EQUAL_CHAR(0, pure::keyboardCharAt(0,    0,    false));   // top-left
    TEST_ASSERT_EQUAL_CHAR(0, pure::keyboardCharAt(799,  479,  false));   // bottom-right
    TEST_ASSERT_EQUAL_CHAR(0, pure::keyboardCharAt(34,   220,  false));   // left of row 0
    TEST_ASSERT_EQUAL_CHAR(0, pure::keyboardCharAt(50,   180,  false));   // above row 0
    TEST_ASSERT_EQUAL_CHAR(0, pure::keyboardCharAt(50,   425,  false));   // below row 4 (at 420)
}

void test_past_last_key_in_row_returns_zero() {
    // Row 4 (ZXCVBNM) starts at x=115, has 7 keys, so keys span x=[115, 115+7*68=591).
    // x=600, y=400 is past the last key in that row.
    TEST_ASSERT_EQUAL_CHAR(0, pure::keyboardCharAt(600, 400, false));
    // Row 0 (10 chars) at x=35 spans up to 35+10*68 = 715.
    TEST_ASSERT_EQUAL_CHAR(0, pure::keyboardCharAt(720, 220, false));
}

void test_m_key_visible_caps_off() {
    // Regression: the v28 fix moved CAPS so the M key is reachable.
    char m = pure::keyboardCharAt(center(115, 6), rowMid(380), false);
    TEST_ASSERT_EQUAL_CHAR('m', m);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_every_key_in_every_row_caps_off);
    RUN_TEST(test_letters_uppercased_when_caps_on);
    RUN_TEST(test_symbols_unaffected_by_caps);
    RUN_TEST(test_out_of_bounds_returns_zero);
    RUN_TEST(test_past_last_key_in_row_returns_zero);
    RUN_TEST(test_m_key_visible_caps_off);
    return UNITY_END();
}

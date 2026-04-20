#include <unity.h>
#include "pure/semver.h"

void setUp() {}
void tearDown() {}

void test_basic_ordering() {
    TEST_ASSERT_TRUE (pure::isNewerVersion("1.0.1", "1.0.0"));
    TEST_ASSERT_TRUE (pure::isNewerVersion("1.1.0", "1.0.1"));
    TEST_ASSERT_TRUE (pure::isNewerVersion("2.0.0", "1.1.0"));
    TEST_ASSERT_FALSE(pure::isNewerVersion("1.0.0", "1.0.1"));
    TEST_ASSERT_FALSE(pure::isNewerVersion("1.0.1", "1.1.0"));
    TEST_ASSERT_FALSE(pure::isNewerVersion("1.1.0", "2.0.0"));
}

void test_equal_is_not_newer() {
    TEST_ASSERT_FALSE(pure::isNewerVersion("1.2.3", "1.2.3"));
    TEST_ASSERT_FALSE(pure::isNewerVersion("0.0.0", "0.0.0"));
}

void test_v_prefix() {
    TEST_ASSERT_TRUE (pure::isNewerVersion("v1.0.1", "1.0.0"));
    TEST_ASSERT_TRUE (pure::isNewerVersion("1.0.1", "v1.0.0"));
    TEST_ASSERT_FALSE(pure::isNewerVersion("v1.0.0", "v1.0.0"));
}

void test_unknown_current_always_older() {
    TEST_ASSERT_TRUE(pure::isNewerVersion("0.0.1", "unknown"));
    TEST_ASSERT_TRUE(pure::isNewerVersion("1.0.15", "unknown"));
}

void test_parse_components() {
    int M, m, p;
    pure::parseVersion("1.2.3", M, m, p);
    TEST_ASSERT_EQUAL_INT(1, M);
    TEST_ASSERT_EQUAL_INT(2, m);
    TEST_ASSERT_EQUAL_INT(3, p);

    pure::parseVersion("v10.20.30", M, m, p);
    TEST_ASSERT_EQUAL_INT(10, M);
    TEST_ASSERT_EQUAL_INT(20, m);
    TEST_ASSERT_EQUAL_INT(30, p);
}

void test_parse_partial() {
    int M, m, p;
    pure::parseVersion("5", M, m, p);
    TEST_ASSERT_EQUAL_INT(5, M);
    TEST_ASSERT_EQUAL_INT(0, m);
    TEST_ASSERT_EQUAL_INT(0, p);

    pure::parseVersion("1.2", M, m, p);
    TEST_ASSERT_EQUAL_INT(1, M);
    TEST_ASSERT_EQUAL_INT(2, m);
    TEST_ASSERT_EQUAL_INT(0, p);
}

void test_parse_malformed_safe() {
    int M, m, p;
    pure::parseVersion("", M, m, p);
    TEST_ASSERT_EQUAL_INT(0, M);
    TEST_ASSERT_EQUAL_INT(0, m);
    TEST_ASSERT_EQUAL_INT(0, p);

    pure::parseVersion("garbage", M, m, p);
    TEST_ASSERT_EQUAL_INT(0, M);

    pure::parseVersion(nullptr, M, m, p);
    TEST_ASSERT_EQUAL_INT(0, M);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_basic_ordering);
    RUN_TEST(test_equal_is_not_newer);
    RUN_TEST(test_v_prefix);
    RUN_TEST(test_unknown_current_always_older);
    RUN_TEST(test_parse_components);
    RUN_TEST(test_parse_partial);
    RUN_TEST(test_parse_malformed_safe);
    return UNITY_END();
}

#include <unity.h>
#include "pure/mqtt_parse.h"

void setUp() {}
void tearDown() {}

static const int NUM_FRIENDS = 5;
static const int OWN_IDX     = 1;  // pretend we are friend index 1

void test_valid_committed() {
    int idx; bool committed;
    TEST_ASSERT_TRUE(pure::parseMqttCommit("FRIYAY:2:1", NUM_FRIENDS, OWN_IDX, idx, committed));
    TEST_ASSERT_EQUAL_INT(2, idx);
    TEST_ASSERT_TRUE(committed);
}

void test_valid_uncommitted() {
    int idx; bool committed;
    TEST_ASSERT_TRUE(pure::parseMqttCommit("FRIYAY:0:0", NUM_FRIENDS, OWN_IDX, idx, committed));
    TEST_ASSERT_EQUAL_INT(0, idx);
    TEST_ASSERT_FALSE(committed);
}

void test_invalid_prefix_rejected() {
    int idx; bool committed;
    TEST_ASSERT_FALSE(pure::parseMqttCommit("HELLO:2:1",   NUM_FRIENDS, OWN_IDX, idx, committed));
    TEST_ASSERT_FALSE(pure::parseMqttCommit("friyay:2:1",  NUM_FRIENDS, OWN_IDX, idx, committed));
    TEST_ASSERT_FALSE(pure::parseMqttCommit("",            NUM_FRIENDS, OWN_IDX, idx, committed));
}

void test_out_of_range_index_rejected() {
    int idx; bool committed;
    TEST_ASSERT_FALSE(pure::parseMqttCommit("FRIYAY:5:1",  NUM_FRIENDS, OWN_IDX, idx, committed));
    TEST_ASSERT_FALSE(pure::parseMqttCommit("FRIYAY:99:1", NUM_FRIENDS, OWN_IDX, idx, committed));
    TEST_ASSERT_FALSE(pure::parseMqttCommit("FRIYAY:-1:1", NUM_FRIENDS, OWN_IDX, idx, committed));
}

void test_own_echo_skipped() {
    int idx; bool committed;
    TEST_ASSERT_FALSE(pure::parseMqttCommit("FRIYAY:1:1", NUM_FRIENDS, OWN_IDX, idx, committed));
}

void test_own_echo_disabled_when_own_idx_is_minus_one() {
    int idx; bool committed;
    TEST_ASSERT_TRUE(pure::parseMqttCommit("FRIYAY:1:1", NUM_FRIENDS, -1, idx, committed));
    TEST_ASSERT_EQUAL_INT(1, idx);
}

void test_missing_second_colon() {
    int idx; bool committed;
    TEST_ASSERT_FALSE(pure::parseMqttCommit("FRIYAY:2",   NUM_FRIENDS, OWN_IDX, idx, committed));
    TEST_ASSERT_FALSE(pure::parseMqttCommit("FRIYAY:21",  NUM_FRIENDS, OWN_IDX, idx, committed));
}

void test_missing_index() {
    int idx; bool committed;
    TEST_ASSERT_FALSE(pure::parseMqttCommit("FRIYAY::1", NUM_FRIENDS, OWN_IDX, idx, committed));
}

void test_invalid_value() {
    int idx; bool committed;
    TEST_ASSERT_FALSE(pure::parseMqttCommit("FRIYAY:2:5", NUM_FRIENDS, OWN_IDX, idx, committed));
    TEST_ASSERT_FALSE(pure::parseMqttCommit("FRIYAY:2:x", NUM_FRIENDS, OWN_IDX, idx, committed));
    TEST_ASSERT_FALSE(pure::parseMqttCommit("FRIYAY:2:",  NUM_FRIENDS, OWN_IDX, idx, committed));
}

void test_null_input_safe() {
    int idx; bool committed;
    TEST_ASSERT_FALSE(pure::parseMqttCommit(nullptr, NUM_FRIENDS, OWN_IDX, idx, committed));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_committed);
    RUN_TEST(test_valid_uncommitted);
    RUN_TEST(test_invalid_prefix_rejected);
    RUN_TEST(test_out_of_range_index_rejected);
    RUN_TEST(test_own_echo_skipped);
    RUN_TEST(test_own_echo_disabled_when_own_idx_is_minus_one);
    RUN_TEST(test_missing_second_colon);
    RUN_TEST(test_missing_index);
    RUN_TEST(test_invalid_value);
    RUN_TEST(test_null_input_safe);
    return UNITY_END();
}

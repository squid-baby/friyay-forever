#include <unity.h>
#include "pure/loop_stats.h"

void setUp() {}
void tearDown() {}

void test_init_zeroes_aggregates() {
    pure::LoopStats s;
    pure::initLoopStats(s, 100);
    TEST_ASSERT_EQUAL_UINT32(100, s.thresholdMs);
    TEST_ASSERT_EQUAL_UINT32(0, s.sampleCount);
    TEST_ASSERT_EQUAL_UINT32(0, s.slowCount);
    TEST_ASSERT_EQUAL_UINT32(0, s.maxMs);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)s.sumMs);
    TEST_ASSERT_EQUAL_UINT32(0, pure::meanTickMs(s));
    TEST_ASSERT_EQUAL_UINT32(0, pure::slowTickBasisPoints(s));
}

void test_record_tick_tracks_max_and_sum() {
    pure::LoopStats s;
    pure::initLoopStats(s, 100);
    pure::recordTick(s, 10);
    pure::recordTick(s, 30);
    pure::recordTick(s, 20);
    TEST_ASSERT_EQUAL_UINT32(3, s.sampleCount);
    TEST_ASSERT_EQUAL_UINT32(30, s.maxMs);
    TEST_ASSERT_EQUAL_UINT32(60, (uint32_t)s.sumMs);
    TEST_ASSERT_EQUAL_UINT32(20, pure::meanTickMs(s));
}

void test_slow_count_uses_strict_greater_than() {
    pure::LoopStats s;
    pure::initLoopStats(s, 100);
    pure::recordTick(s, 100);   // exactly threshold — NOT slow
    pure::recordTick(s, 101);   // slow
    pure::recordTick(s, 5000);  // slow (a real freeze)
    TEST_ASSERT_EQUAL_UINT32(3, s.sampleCount);
    TEST_ASSERT_EQUAL_UINT32(2, s.slowCount);
    TEST_ASSERT_EQUAL_UINT32(5000, s.maxMs);
}

void test_slow_basis_points() {
    pure::LoopStats s;
    pure::initLoopStats(s, 100);
    // 1 slow out of 10000 samples = 1 bp (0.01%)
    for (int i = 0; i < 9999; i++) pure::recordTick(s, 10);
    pure::recordTick(s, 500);
    TEST_ASSERT_EQUAL_UINT32(10000, s.sampleCount);
    TEST_ASSERT_EQUAL_UINT32(1, s.slowCount);
    TEST_ASSERT_EQUAL_UINT32(1, pure::slowTickBasisPoints(s));
}

void test_sum_does_not_wrap_on_long_session() {
    // 1M ticks of 10ms = 10M ms. 32-bit would hold this, but this exercises
    // the 64-bit accumulator used for longer sessions.
    pure::LoopStats s;
    pure::initLoopStats(s, 100);
    const uint32_t ticks = 1000000;
    for (uint32_t i = 0; i < ticks; i++) pure::recordTick(s, 10);
    TEST_ASSERT_EQUAL_UINT32(ticks, s.sampleCount);
    TEST_ASSERT_EQUAL_UINT32(10, pure::meanTickMs(s));
    TEST_ASSERT_EQUAL_UINT64(10000000ULL, s.sumMs);
}

void test_threshold_can_be_reconfigured() {
    pure::LoopStats s;
    pure::initLoopStats(s, 50);
    pure::recordTick(s, 75);  // slow at threshold=50
    TEST_ASSERT_EQUAL_UINT32(1, s.slowCount);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_init_zeroes_aggregates);
    RUN_TEST(test_record_tick_tracks_max_and_sum);
    RUN_TEST(test_slow_count_uses_strict_greater_than);
    RUN_TEST(test_slow_basis_points);
    RUN_TEST(test_sum_does_not_wrap_on_long_session);
    RUN_TEST(test_threshold_can_be_reconfigured);
    return UNITY_END();
}

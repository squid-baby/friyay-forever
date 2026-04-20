#include <unity.h>
#include "pure/weather_logic.h"

void setUp() {}
void tearDown() {}

void test_freezing_boundary_clamps_temp_to_zero() {
    // At exactly 32F, the original logic uses `<= 32` for tmp but `< 32` for
    // tempScore — so tmp clamps to 0 while tempScore is still computed:
    // diff = |32-65| = 33, tempScore = 10 - 33/5 = 4 => fuk = 4.
    int wet, tmp, fuk;
    pure::calcWeatherLevels(32.0f, 0.0f, wet, tmp, fuk);
    TEST_ASSERT_EQUAL_INT(0, wet);
    TEST_ASSERT_EQUAL_INT(0, tmp);
    TEST_ASSERT_EQUAL_INT(4, fuk);
}

void test_hot_boundary_clamps_temp_to_max() {
    // At exactly 100F: tmp = 10. diff = 35, tempScore = 10 - 7 = 3 => fuk = 3.
    int wet, tmp, fuk;
    pure::calcWeatherLevels(100.0f, 0.0f, wet, tmp, fuk);
    TEST_ASSERT_EQUAL_INT(10, tmp);
    TEST_ASSERT_EQUAL_INT(3, fuk);
}

void test_above_100_kills_fuk() {
    // > 100F falls into the strict outside-range branch: tempScore = 0.
    int wet, tmp, fuk;
    pure::calcWeatherLevels(101.0f, 0.0f, wet, tmp, fuk);
    TEST_ASSERT_EQUAL_INT(10, tmp);
    TEST_ASSERT_EQUAL_INT(0, fuk);
}

void test_optimal_65_max_fuk() {
    int wet, tmp, fuk;
    pure::calcWeatherLevels(65.0f, 0.0f, wet, tmp, fuk);
    TEST_ASSERT_EQUAL_INT(0, wet);
    TEST_ASSERT_EQUAL_INT(10, fuk);
}

void test_zero_rain_is_zero_wet() {
    int wet, tmp, fuk;
    pure::calcWeatherLevels(70.0f, 0.0f, wet, tmp, fuk);
    TEST_ASSERT_EQUAL_INT(0, wet);
}

void test_heavy_rain_caps_wet_at_ten() {
    int wet, tmp, fuk;
    // 200mm ~= 7.87in => 7.87 * 5 = 39.3, clamped to 10
    pure::calcWeatherLevels(70.0f, 200.0f, wet, tmp, fuk);
    TEST_ASSERT_EQUAL_INT(10, wet);
}

void test_heavy_rain_kills_fuk_score() {
    int wet, tmp, fuk;
    // perfect 65F but rained out => fuk drops
    pure::calcWeatherLevels(65.0f, 200.0f, wet, tmp, fuk);
    TEST_ASSERT_EQUAL_INT(0, fuk);
}

void test_negative_temp_clamps_to_zero() {
    int wet, tmp, fuk;
    pure::calcWeatherLevels(-10.0f, 0.0f, wet, tmp, fuk);
    TEST_ASSERT_EQUAL_INT(0, tmp);
    TEST_ASSERT_EQUAL_INT(0, fuk);
}

void test_just_above_freezing() {
    int wet, tmp, fuk;
    // 33F: (33-32)/6.8 = 0.147 -> 0
    pure::calcWeatherLevels(33.0f, 0.0f, wet, tmp, fuk);
    TEST_ASSERT_EQUAL_INT(0, tmp);
    // Inside [32, 100]: tempDiff=32, tempScore = 10 - 32/5 = 4
    TEST_ASSERT_EQUAL_INT(4, fuk);
}

void test_warm_day_no_rain_is_high_fuk() {
    int wet, tmp, fuk;
    // 70F: tempDiff=5, tempScore=9
    pure::calcWeatherLevels(70.0f, 0.0f, wet, tmp, fuk);
    TEST_ASSERT_EQUAL_INT(9, fuk);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_freezing_boundary_clamps_temp_to_zero);
    RUN_TEST(test_hot_boundary_clamps_temp_to_max);
    RUN_TEST(test_above_100_kills_fuk);
    RUN_TEST(test_optimal_65_max_fuk);
    RUN_TEST(test_zero_rain_is_zero_wet);
    RUN_TEST(test_heavy_rain_caps_wet_at_ten);
    RUN_TEST(test_heavy_rain_kills_fuk_score);
    RUN_TEST(test_negative_temp_clamps_to_zero);
    RUN_TEST(test_just_above_freezing);
    RUN_TEST(test_warm_day_no_rain_is_high_fuk);
    return UNITY_END();
}

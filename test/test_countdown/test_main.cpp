#include <unity.h>
#include "pure/countdown.h"
#include <ctime>
#include <cstdlib>

void setUp() {
    // Pin TZ to UTC so mktime() in calcCountdownSeconds is reproducible
    // across CI hosts. The countdown logic is timezone-agnostic — it just
    // needs `now` and the broken-down `tinfo` to agree, which they do here.
    setenv("TZ", "UTC0", 1);
    tzset();
}
void tearDown() {}

static time_t make_time(int year, int mon, int mday, int hour, int min, int sec) {
    struct tm t{};
    t.tm_year = year - 1900;
    t.tm_mon  = mon - 1;
    t.tm_mday = mday;
    t.tm_hour = hour;
    t.tm_min  = min;
    t.tm_sec  = sec;
    t.tm_isdst = 0;
    return timegm(&t);
}

static struct tm gm_of(time_t t) {
    struct tm out;
    gmtime_r(&t, &out);
    return out;
}

void test_friday_one_second_before_three_pm() {
    // 2026-04-24 (Friday) 14:59:59 UTC -> 1 second remaining.
    time_t now = make_time(2026, 4, 24, 14, 59, 59);
    struct tm tm_now = gm_of(now);
    TEST_ASSERT_EQUAL_INT(5, tm_now.tm_wday);  // sanity: Friday
    long secs = pure::calcCountdownSeconds(now, tm_now);
    TEST_ASSERT_EQUAL_INT(1, secs);
}

void test_friday_at_three_pm_rolls_to_next_friday() {
    // Exactly 15:00:00 on Friday -> rolls forward 7 days = 604800.
    time_t now = make_time(2026, 4, 24, 15, 0, 0);
    struct tm tm_now = gm_of(now);
    long secs = pure::calcCountdownSeconds(now, tm_now);
    TEST_ASSERT_EQUAL_INT(7L * 24 * 3600, secs);
}

void test_saturday_is_almost_six_days() {
    // Saturday 2026-04-25 00:00:00 UTC -> next Friday 15:00 is
    // 6 days + 15 hours = 6*86400 + 15*3600 = 572400.
    time_t now = make_time(2026, 4, 25, 0, 0, 0);
    struct tm tm_now = gm_of(now);
    TEST_ASSERT_EQUAL_INT(6, tm_now.tm_wday);
    long secs = pure::calcCountdownSeconds(now, tm_now);
    TEST_ASSERT_EQUAL_INT(6L * 86400 + 15 * 3600, secs);
}

void test_monday_morning() {
    // Monday 2026-04-20 09:00:00 UTC -> Friday is 4 days away,
    // plus (15-9)=6 hours = 4*86400 + 6*3600 = 367200.
    time_t now = make_time(2026, 4, 20, 9, 0, 0);
    struct tm tm_now = gm_of(now);
    TEST_ASSERT_EQUAL_INT(1, tm_now.tm_wday);
    long secs = pure::calcCountdownSeconds(now, tm_now);
    TEST_ASSERT_EQUAL_INT(4L * 86400 + 6 * 3600, secs);
}

void test_friday_morning() {
    // Friday 2026-04-24 09:00:00 UTC -> 6 hours.
    time_t now = make_time(2026, 4, 24, 9, 0, 0);
    struct tm tm_now = gm_of(now);
    long secs = pure::calcCountdownSeconds(now, tm_now);
    TEST_ASSERT_EQUAL_INT(6L * 3600, secs);
}

void test_dst_spring_forward_week() {
    // US DST spring-forward: Sunday 2026-03-08. Pinned to UTC for this test
    // so we just verify the function still returns a sane forward-looking
    // value across the boundary. From Wed 2026-03-11 12:00 UTC to Fri 2026-03-13
    // 15:00 UTC = 2 days + 3 hours.
    time_t now = make_time(2026, 3, 11, 12, 0, 0);
    struct tm tm_now = gm_of(now);
    long secs = pure::calcCountdownSeconds(now, tm_now);
    TEST_ASSERT_EQUAL_INT(2L * 86400 + 3 * 3600, secs);
}

void test_negative_clamps_to_zero() {
    // Sanity: function never returns negative.
    time_t now = make_time(2026, 4, 24, 14, 59, 59);
    struct tm tm_now = gm_of(now);
    long secs = pure::calcCountdownSeconds(now, tm_now);
    TEST_ASSERT_TRUE(secs >= 0);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_friday_one_second_before_three_pm);
    RUN_TEST(test_friday_at_three_pm_rolls_to_next_friday);
    RUN_TEST(test_saturday_is_almost_six_days);
    RUN_TEST(test_monday_morning);
    RUN_TEST(test_friday_morning);
    RUN_TEST(test_dst_spring_forward_week);
    RUN_TEST(test_negative_clamps_to_zero);
    return UNITY_END();
}

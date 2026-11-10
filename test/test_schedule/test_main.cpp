#include <unity.h>

#include <cstdlib>
#include <ctime>

#include "schedule.h"

static const char* TZ = "CET-1CEST,M3.5.0,M10.5.0/3";

static int64_t local(int y, int mo, int d, int h, int mi) {
    setenv("TZ", TZ, 1);
    tzset();
    tm t{};
    t.tm_year = y - 1900;
    t.tm_mon = mo - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min = mi;
    t.tm_isdst = -1;
    return mktime(&t);
}

void setUp() {}
void tearDown() {}

void test_plan_day_normal() {
    Plan p = planAfterDayRefresh(local(2026, 11, 10, 10, 0), TZ);
    TEST_ASSERT_TRUE(p.phase == Phase::Day);
    TEST_ASSERT_EQUAL(0, p.shift);
    TEST_ASSERT_EQUAL_INT64(900, p.sleepSeconds);
}
void test_plan_day_before_22() {
    Plan p = planAfterDayRefresh(local(2026, 11, 10, 21, 52), TZ);
    TEST_ASSERT_TRUE(p.phase == Phase::Day);
    TEST_ASSERT_EQUAL_INT64(480, p.sleepSeconds);
}
void test_plan_night_at_22() {
    Plan p = planNight(local(2026, 11, 10, 22, 0), TZ);
    TEST_ASSERT_TRUE(p.phase == Phase::Night);
    TEST_ASSERT_EQUAL(1, p.shift);
    TEST_ASSERT_EQUAL_INT64(8 * 3600, p.sleepSeconds);
}
void test_plan_night_after_midnight() {
    Plan p = planNight(local(2026, 11, 11, 0, 30), TZ);
    TEST_ASSERT_EQUAL(0, p.shift);
    TEST_ASSERT_EQUAL_INT64(5 * 3600 + 1800, p.sleepSeconds);
}
void test_plan_dst_spring() {
    Plan p = planNight(local(2027, 3, 27, 22, 0), TZ);
    TEST_ASSERT_EQUAL_INT64(7 * 3600, p.sleepSeconds);
}
void test_plan_dst_autumn() {
    Plan p = planNight(local(2026, 10, 24, 22, 0), TZ);
    TEST_ASSERT_EQUAL_INT64(9 * 3600, p.sleepSeconds);
}
void test_phase() {
    TEST_ASSERT_TRUE(phaseAt(local(2026, 11, 10, 5, 59), TZ) == Phase::Night);
    TEST_ASSERT_TRUE(phaseAt(local(2026, 11, 10, 6, 0), TZ) == Phase::Day);
    TEST_ASSERT_TRUE(phaseAt(local(2026, 11, 10, 21, 59), TZ) == Phase::Day);
    TEST_ASSERT_TRUE(phaseAt(local(2026, 11, 10, 22, 0), TZ) == Phase::Night);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_plan_day_normal);
    RUN_TEST(test_plan_day_before_22);
    RUN_TEST(test_plan_night_at_22);
    RUN_TEST(test_plan_night_after_midnight);
    RUN_TEST(test_plan_dst_spring);
    RUN_TEST(test_plan_dst_autumn);
    RUN_TEST(test_phase);
    return UNITY_END();
}

#include <unity.h>

#include <cstdlib>
#include <ctime>

#include "wake.h"

void setUp() {}
void tearDown() {}

static WakeInput base() { return WakeInput{}; }

void test_battery_critical_wins() {
    WakeInput i = base();
    i.batteryPct = 4;
    i.confirm = true;
    TEST_ASSERT_TRUE(decideWake(i) == WakeAction::ChargeMe);
    i.batteryPct = 5;
    TEST_ASSERT_TRUE(decideWake(i) == WakeAction::Maintenance);
}

void test_confirm_or_missing_settings_is_maintenance() {
    WakeInput i = base();
    i.confirm = true;
    TEST_ASSERT_TRUE(decideWake(i) == WakeAction::Maintenance);
    i = base();
    i.settingsComplete = false;
    TEST_ASSERT_TRUE(decideWake(i) == WakeAction::Maintenance);
}

void test_unknown_time_fetches_as_day() {
    WakeInput i = base();
    i.timeKnown = false;
    i.phase = Phase::Night;
    TEST_ASSERT_TRUE(decideWake(i) == WakeAction::Refresh);
}

void test_short_day_sleep() {
    TEST_ASSERT_TRUE(dayPlanTooShort(Plan{Phase::Day, 0, 59}));
    TEST_ASSERT_FALSE(dayPlanTooShort(Plan{Phase::Day, 0, 60}));
}

static const char* TZ = "CET-1CEST,M3.5.0,M10.5.0/3";

void test_server_time_plausible() {
    TEST_ASSERT_FALSE(serverTimePlausible(0));
    TEST_ASSERT_FALSE(serverTimePlausible(1767225599));
    TEST_ASSERT_TRUE(serverTimePlausible(1767225600));
}

void test_charge_me_plan() {
    ChargeMePlan p = planChargeMe(false, false, false);
    TEST_ASSERT_TRUE(p.draw);
    TEST_ASSERT_TRUE(p.wakeOnCharger);
    TEST_ASSERT_EQUAL_INT64(0, p.sleepSeconds);
    p = planChargeMe(true, false, false);
    TEST_ASSERT_TRUE(p.draw);
    TEST_ASSERT_FALSE(p.wakeOnCharger);
    TEST_ASSERT_EQUAL_INT64(900, p.sleepSeconds);
    p = planChargeMe(true, true, true);
    TEST_ASSERT_FALSE(p.draw);
    TEST_ASSERT_FALSE(p.wakeOnCharger);
    p = planChargeMe(false, true, true);
    TEST_ASSERT_FALSE(p.draw);
    TEST_ASSERT_FALSE(p.wakeOnCharger);
    TEST_ASSERT_EQUAL_INT64(900, p.sleepSeconds);
}

static int64_t local(int y, int mo, int d, int h, int mi, int s = 0) {
    setenv("TZ", TZ, 1);
    tzset();
    tm t{};
    t.tm_year = y - 1900;
    t.tm_mon = mo - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min = mi;
    t.tm_sec = s;
    t.tm_isdst = -1;
    return mktime(&t);
}

void test_plan_normal_day() {
    WakePlan p = planWake(local(2026, 11, 10, 10, 0), TZ, false);
    TEST_ASSERT_FALSE(p.renderNight);
    TEST_ASSERT_EQUAL(0, p.shift);
    TEST_ASSERT_EQUAL_INT64(900, p.sleepSeconds);
}

void test_plan_day_close_to_22() {
    int64_t now = local(2026, 11, 10, 21, 59, 30);
    WakePlan p = planWake(now, TZ, false);
    TEST_ASSERT_TRUE(p.renderNight);
    TEST_ASSERT_EQUAL(1, p.shift);
    TEST_ASSERT_EQUAL_INT64(local(2026, 11, 11, 6, 0) - now, p.sleepSeconds);
}

void test_plan_exactly_22() {
    int64_t now = local(2026, 11, 10, 22, 0);
    WakePlan p = planWake(now, TZ, false);
    TEST_ASSERT_TRUE(p.renderNight);
    TEST_ASSERT_EQUAL(1, p.shift);
    TEST_ASSERT_EQUAL_INT64(8 * 3600, p.sleepSeconds);
}

void test_plan_after_midnight() {
    int64_t now = local(2026, 11, 11, 1, 0);
    WakePlan p = planWake(now, TZ, false);
    TEST_ASSERT_TRUE(p.renderNight);
    TEST_ASSERT_EQUAL(0, p.shift);
    TEST_ASSERT_EQUAL_INT64(5 * 3600, p.sleepSeconds);
}

void test_plan_night_down() {
    WakePlan p = planWake(local(2026, 11, 10, 23, 0), TZ, true);
    TEST_ASSERT_FALSE(p.renderNight);
    TEST_ASSERT_EQUAL(0, p.shift);
    TEST_ASSERT_EQUAL_INT64(600, p.sleepSeconds);
}

void test_plan_day_down_is_normal() {
    WakePlan p = planWake(local(2026, 11, 10, 10, 0), TZ, true);
    TEST_ASSERT_FALSE(p.renderNight);
    TEST_ASSERT_EQUAL_INT64(900, p.sleepSeconds);
}

void test_plan_night_near_6_is_day() {
    int64_t now = local(2026, 11, 11, 5, 59, 30);
    WakePlan p = planWake(now, TZ, false);
    TEST_ASSERT_FALSE(p.renderNight);
    TEST_ASSERT_EQUAL(0, p.shift);
    TEST_ASSERT_TRUE(now + p.sleepSeconds > local(2026, 11, 11, 6, 0));
    TEST_ASSERT_TRUE(p.sleepSeconds < 120);
}

void test_plan_night_just_over_minimum() {
    WakePlan p = planWake(local(2026, 11, 11, 5, 58, 50), TZ, false);
    TEST_ASSERT_TRUE(p.renderNight);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_plan_normal_day);
    RUN_TEST(test_plan_day_close_to_22);
    RUN_TEST(test_plan_exactly_22);
    RUN_TEST(test_plan_after_midnight);
    RUN_TEST(test_plan_night_down);
    RUN_TEST(test_plan_day_down_is_normal);
    RUN_TEST(test_plan_night_near_6_is_day);
    RUN_TEST(test_plan_night_just_over_minimum);

    RUN_TEST(test_battery_critical_wins);
    RUN_TEST(test_confirm_or_missing_settings_is_maintenance);
    RUN_TEST(test_unknown_time_fetches_as_day);
    RUN_TEST(test_short_day_sleep);
    RUN_TEST(test_server_time_plausible);
    RUN_TEST(test_charge_me_plan);
    return UNITY_END();
}

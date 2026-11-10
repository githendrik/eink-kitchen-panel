#include <unity.h>

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

void test_night_timer_shows_night() {
    WakeInput i = base();
    i.phase = Phase::Night;
    TEST_ASSERT_TRUE(decideWake(i) == WakeAction::Refresh);
    i.down = false;
    TEST_ASSERT_TRUE(wantsNightScreen(i));
}

void test_night_down_is_live_day() {
    WakeInput i = base();
    i.phase = Phase::Night;
    i.down = true;
    TEST_ASSERT_FALSE(wantsNightScreen(i));
}

void test_day_never_night_screen() {
    WakeInput i = base();
    TEST_ASSERT_FALSE(wantsNightScreen(i));
}

void test_short_day_sleep() {
    TEST_ASSERT_TRUE(dayPlanTooShort(Plan{Phase::Day, 0, 59}));
    TEST_ASSERT_FALSE(dayPlanTooShort(Plan{Phase::Day, 0, 60}));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_battery_critical_wins);
    RUN_TEST(test_confirm_or_missing_settings_is_maintenance);
    RUN_TEST(test_unknown_time_fetches_as_day);
    RUN_TEST(test_night_timer_shows_night);
    RUN_TEST(test_night_down_is_live_day);
    RUN_TEST(test_day_never_night_screen);
    RUN_TEST(test_short_day_sleep);
    return UNITY_END();
}

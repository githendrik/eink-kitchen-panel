#include <unity.h>

#include <cstdlib>
#include <ctime>

#include "view.h"

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

static Kitchen base(int64_t now) {
    Kitchen k;
    k.today.date = "2026-11-10";
    k.tomorrow.date = "2026-11-11";
    Solar s;
    s.pvW = 2400;
    s.loadW = 900;
    s.gridW = -1100;
    s.batterySoc = 78;
    s.batteryW = 0;
    s.updatedAtUtc = now - 60;
    k.solar = s;
    return k;
}

void setUp() {}
void tearDown() {}

static const int64_t NOW = 1794301200;  // overwritten in tests via local()

void test_export() {
    int64_t now = local(2026, 11, 10, 10, 0);
    View v = buildDayView(base(now), now, TZ, 80, now);
    TEST_ASSERT_EQUAL_STRING("Netz → 1.1 kW", v.gridText.c_str());
    TEST_ASSERT_EQUAL_STRING("2.4 kW", v.pvText.c_str());
    TEST_ASSERT_EQUAL_STRING("Haus 0.9 kW", v.houseText.c_str());
    TEST_ASSERT_FALSE(v.warnGridImport);
    TEST_ASSERT_EQUAL_STRING("Di 10. Nov", v.headerDate.c_str());
    TEST_ASSERT_EQUAL_STRING("Heute", v.todayLabel.c_str());
    TEST_ASSERT_EQUAL_STRING("10:00", v.updatedAt.c_str());
}

void test_import_and_threshold() {
    int64_t now = local(2026, 11, 10, 10, 0);
    Kitchen k = base(now);
    k.solar->gridW = 600;
    View v = buildDayView(k, now, TZ, 80, now);
    TEST_ASSERT_TRUE(v.warnGridImport);
    TEST_ASSERT_EQUAL_STRING("Netz ← 0.6 kW", v.gridText.c_str());
    k.solar->gridW = 30;
    TEST_ASSERT_FALSE(buildDayView(k, now, TZ, 80, now).warnGridImport);
}

void test_battery_warnings_and_arrow() {
    int64_t now = local(2026, 11, 10, 10, 0);
    Kitchen k = base(now);
    k.solar->batterySoc = 12;
    k.solar->batteryW = 300;
    View v = buildDayView(k, now, TZ, 15, now);
    TEST_ASSERT_TRUE(v.warnHomeBatteryLow);
    TEST_ASSERT_TRUE(v.warnPanelBattery);
    TEST_ASSERT_EQUAL_STRING("↑", v.batteryArrow.c_str());
    k.solar->batteryW = -300;
    TEST_ASSERT_EQUAL_STRING("↓", buildDayView(k, now, TZ, 80, now).batteryArrow.c_str());
    k.solar->batteryW = 49;
    TEST_ASSERT_EQUAL_STRING("", buildDayView(k, now, TZ, 80, now).batteryArrow.c_str());
}

void test_solar_missing_and_stale() {
    int64_t now = local(2026, 11, 10, 10, 0);
    Kitchen k = base(now);
    k.solar.reset();
    TEST_ASSERT_TRUE(buildDayView(k, now, TZ, 80, now).solarMissing);
    k = base(now);
    k.solar->updatedAtUtc = now - 40 * 60;
    View v = buildDayView(k, now, TZ, 80, now);
    TEST_ASSERT_TRUE(v.solarStale);
    TEST_ASSERT_EQUAL_STRING("Stand 09:20", v.solarStandText.c_str());
}

void test_data_stale() {
    int64_t now = local(2026, 11, 10, 10, 0);
    View v = buildDayView(base(now), now, TZ, 80, now - 50 * 60);
    TEST_ASSERT_TRUE(v.dataStale);
    TEST_ASSERT_EQUAL_STRING("Stand 09:10", v.staleText.c_str());
    TEST_ASSERT_FALSE(buildDayView(base(now), now, TZ, 80, 0).dataStale);
    TEST_ASSERT_FALSE(buildDayView(base(now), now, TZ, 80, now - 10 * 60).dataStale);
}

void test_events() {
    int64_t now = local(2026, 11, 10, 10, 0);
    Kitchen k = base(now);
    k.today.events.push_back({"Zahnarzt", "2026-11-10T08:00:00", "2026-11-10T09:00:00", false});
    k.today.events.push_back({"Geburtstag", "2026-11-10", "2026-11-11", true});
    View v = buildDayView(k, now, TZ, 80, now);
    TEST_ASSERT_EQUAL(2, (int)v.today.size());
    TEST_ASSERT_EQUAL_STRING("ganztägig", v.today[0].time.c_str());
    TEST_ASSERT_EQUAL_STRING("Geburtstag", v.today[0].title.c_str());
    TEST_ASSERT_EQUAL_STRING("08:00", v.today[1].time.c_str());
}

void test_german_dates() {
    int64_t now = local(2026, 11, 10, 10, 0);
    Kitchen k = base(now);
    k.today.date = "2026-03-09";
    TEST_ASSERT_EQUAL_STRING("Mo 9. Mär", buildDayView(k, now, TZ, 80, now).headerDate.c_str());
    k.today.date = "2026-12-27";
    TEST_ASSERT_EQUAL_STRING("So 27. Dez", buildDayView(k, now, TZ, 80, now).headerDate.c_str());
}

void test_night_labels() {
    int64_t now = local(2026, 11, 10, 22, 0);
    Kitchen k = base(now);
    k.today.date = "2026-11-11";
    k.tomorrow.date = "2026-11-12";
    View v = buildNightView(k, now, TZ, 80);
    TEST_ASSERT_TRUE(v.screen == Screen::Night);
    TEST_ASSERT_EQUAL_STRING("Morgen", v.todayLabel.c_str());
    TEST_ASSERT_EQUAL_STRING("Übermorgen", v.tomorrowLabel.c_str());
    int64_t after = local(2026, 11, 11, 3, 0);
    k.today.date = "2026-11-11";
    TEST_ASSERT_EQUAL_STRING("Heute", buildNightView(k, after, TZ, 80).todayLabel.c_str());
}

static uint32_t fp(const Kitchen& k, int64_t now) { return fingerprint(buildDayView(k, now, TZ, 80, now)); }

void test_fingerprint() {
    int64_t t1 = local(2026, 11, 10, 10, 0), t2 = local(2026, 11, 10, 10, 15);
    TEST_ASSERT_EQUAL_UINT32(fp(base(t1), t1), fp(base(t2), t2));
    Kitchen a = base(t1), b = base(t1);
    b.solar->pvW = 2440;
    TEST_ASSERT_EQUAL_UINT32(fp(a, t1), fp(b, t1));
    b.solar->pvW = 2500;
    TEST_ASSERT_NOT_EQUAL(fp(a, t1), fp(b, t1));
    b = base(t1);
    b.solar->batterySoc = 79;
    TEST_ASSERT_EQUAL_UINT32(fp(a, t1), fp(b, t1));
    b.solar->batterySoc = 81;
    TEST_ASSERT_NOT_EQUAL(fp(a, t1), fp(b, t1));
    a.today.events.push_back({"A", "2026-11-10T08:00:00", "", false});
    b = a;
    b.today.events[0].summary = "B";
    TEST_ASSERT_NOT_EQUAL(fp(a, t1), fp(b, t1));
}

int main() {
    (void)NOW;
    UNITY_BEGIN();
    RUN_TEST(test_export);
    RUN_TEST(test_import_and_threshold);
    RUN_TEST(test_battery_warnings_and_arrow);
    RUN_TEST(test_solar_missing_and_stale);
    RUN_TEST(test_data_stale);
    RUN_TEST(test_events);
    RUN_TEST(test_german_dates);
    RUN_TEST(test_night_labels);
    RUN_TEST(test_fingerprint);
    return UNITY_END();
}

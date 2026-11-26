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

// History for 2026-11-10 with n bins at pv/load units (100 W).
static History hist(int n, int pv, int load, int pvWh = 40460, int loadWh = 11800) {
    History h;
    h.date = "2026-11-10";
    h.pv.assign(n, pv);
    h.load.assign(n, load);
    h.pvWh = pvWh;
    h.loadWh = loadWh;
    return h;
}

void test_chart_present_for_today() {
    int64_t now = local(2026, 11, 10, 20, 30);
    Kitchen k = base(now);
    k.history = hist(170, 10, 4);
    k.history->pv[80] = 55;  // 5.5 kW peak
    k.history->load[3] = kNoData;
    View v = buildDayView(k, now, TZ, 80, now);
    TEST_ASSERT_TRUE(v.chart.present);
    TEST_ASSERT_EQUAL_STRING("Erzeugt 40 kWh · Verbraucht 12 kWh", v.chart.totalsText.c_str());
    TEST_ASSERT_EQUAL_STRING("6 kW", v.chart.scaleText.c_str());
    TEST_ASSERT_EQUAL(6, v.chart.scaleKw);
    TEST_ASSERT_EQUAL(20 * 60 + 30, v.chart.nowMin);
    TEST_ASSERT_EQUAL(360, v.chart.startMin);
    TEST_ASSERT_EQUAL(5, v.chart.stepMin);
    TEST_ASSERT_EQUAL(170, (int)v.chart.pvW.size());
    TEST_ASSERT_EQUAL(5500, v.chart.pvW[80]);
    TEST_ASSERT_EQUAL(400, v.chart.loadW[0]);
    TEST_ASSERT_EQUAL(-1, v.chart.loadW[3]);
}

void test_chart_totals_one_decimal_below_10() {
    int64_t now = local(2026, 11, 10, 9, 0);
    Kitchen k = base(now);
    k.history = hist(36, 5, 4, 3240, 9960);
    View v = buildDayView(k, now, TZ, 80, now);
    TEST_ASSERT_EQUAL_STRING("Erzeugt 3.2 kWh · Verbraucht 10 kWh", v.chart.totalsText.c_str());
}

void test_chart_scale_uses_load_peak() {
    int64_t now = local(2026, 11, 10, 12, 0);
    Kitchen k = base(now);
    k.history = hist(72, 21, 10);
    k.history->load[5] = 34;
    TEST_ASSERT_EQUAL_STRING("4 kW", buildDayView(k, now, TZ, 80, now).chart.scaleText.c_str());
}

void test_chart_scale_min_1() {
    int64_t now = local(2026, 11, 10, 6, 3);
    Kitchen k = base(now);
    k.history = hist(0, 0, 0, 0, 0);
    View v = buildDayView(k, now, TZ, 80, now);
    TEST_ASSERT_TRUE(v.chart.present);
    TEST_ASSERT_EQUAL_STRING("1 kW", v.chart.scaleText.c_str());
    TEST_ASSERT_EQUAL_STRING("Erzeugt 0.0 kWh · Verbraucht 0.0 kWh", v.chart.totalsText.c_str());
}

void test_chart_absent_when_date_not_today() {
    int64_t now = local(2026, 11, 11, 7, 0);
    Kitchen k = base(now);
    k.today.date = "2026-11-11";
    k.history = hist(12, 1, 1);  // dated 2026-11-10
    TEST_ASSERT_FALSE(buildDayView(k, now, TZ, 80, now).chart.present);
}

void test_chart_absent_on_night_view() {
    int64_t now = local(2026, 11, 10, 22, 30);
    Kitchen k = base(now);
    k.history = hist(192, 1, 1);
    TEST_ASSERT_FALSE(buildNightView(k, now, TZ, 80).chart.present);
}

void test_fingerprint_changes_when_chart_grows() {
    int64_t now = local(2026, 11, 10, 12, 0);
    Kitchen a = base(now), b = base(now), c = base(now);
    a.history = hist(70, 10, 4);
    b.history = hist(71, 10, 4);
    c.history = hist(70, 10, 4);
    c.history->pv[20] = 11;
    uint32_t fa = fingerprint(buildDayView(a, now, TZ, 80, now));
    TEST_ASSERT_NOT_EQUAL(fa, fingerprint(buildDayView(b, now, TZ, 80, now)));
    TEST_ASSERT_NOT_EQUAL(fa, fingerprint(buildDayView(c, now, TZ, 80, now)));
    TEST_ASSERT_NOT_EQUAL(fa, fingerprint(buildDayView(base(now), now, TZ, 80, now)));
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
    RUN_TEST(test_chart_present_for_today);
    RUN_TEST(test_chart_totals_one_decimal_below_10);
    RUN_TEST(test_chart_scale_uses_load_peak);
    RUN_TEST(test_chart_scale_min_1);
    RUN_TEST(test_chart_absent_when_date_not_today);
    RUN_TEST(test_chart_absent_on_night_view);
    RUN_TEST(test_fingerprint_changes_when_chart_grows);
    return UNITY_END();
}

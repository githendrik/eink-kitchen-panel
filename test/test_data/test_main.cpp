#include <unity.h>

#include <cstring>
#include <string>

#include "battery.h"
#include "kitchen.h"

void setUp() {}
void tearDown() {}

static const char* FULL = R"({
  "calendar": {
    "today": {"label": "Today", "date": "2026-11-10", "events": [
      {"summary": "Dentist", "start": "2026-11-10T14:00:00", "end": "2026-11-10T15:00:00", "allDay": false}]},
    "tomorrow": {"label": "Tomorrow", "date": "2026-11-11", "events": [
      {"summary": "Hochzeitstag", "start": "2026-11-11", "end": "2026-11-12", "allDay": true}]},
    "fetched_at": "2026-11-10T14:22:03.721Z"
  },
  "solar": {"pv_w": 1000, "load_w": 500, "grid_w": -500, "battery_soc": 50, "battery_w": 0,
            "updated_at": "2026-11-10T06:39:12Z", "source": "fake", "stale": false},
  "server_time": "2026-11-10T22:05:00+01:00"
})";

static bool parse(const char* j, Kitchen& k, std::string& err) {
    return parseKitchen(j, strlen(j), k, err);
}

void test_parse_full() {
    Kitchen k;
    std::string err;
    TEST_ASSERT_TRUE(parse(FULL, k, err));
    TEST_ASSERT_EQUAL_STRING("2026-11-10", k.today.date.c_str());
    TEST_ASSERT_EQUAL(1, (int)k.today.events.size());
    TEST_ASSERT_EQUAL_STRING("Dentist", k.today.events[0].summary.c_str());
    TEST_ASSERT_EQUAL_STRING("2026-11-10T14:00:00", k.today.events[0].start.c_str());
    TEST_ASSERT_FALSE(k.today.events[0].allDay);
    TEST_ASSERT_TRUE(k.tomorrow.events[0].allDay);
    TEST_ASSERT_EQUAL_STRING("2026-11-12", k.tomorrow.events[0].end.c_str());
    TEST_ASSERT_TRUE(k.solar.has_value());
    TEST_ASSERT_EQUAL(1000, k.solar->pvW);
    TEST_ASSERT_EQUAL(500, k.solar->loadW);
    TEST_ASSERT_EQUAL(-500, k.solar->gridW);
    TEST_ASSERT_EQUAL(50, k.solar->batterySoc);
    TEST_ASSERT_EQUAL(0, k.solar->batteryW);
    TEST_ASSERT_FALSE(k.solar->stale);
    // 2026-11-10T06:39:12Z
    TEST_ASSERT_EQUAL_INT64(1794292752LL, k.solar->updatedAtUtc);
    // 2026-11-10T21:05:00Z
    TEST_ASSERT_EQUAL_INT64(1794344700LL, k.serverTimeUtc);
}

void test_parse_solar_null() {
    Kitchen k;
    std::string err;
    const char* j = R"({"calendar":{"today":{"date":"a","events":[]},"tomorrow":{"date":"b","events":[]}},"solar":null,"server_time":"2026-11-10T22:05:00+01:00"})";
    TEST_ASSERT_TRUE(parse(j, k, err));
    TEST_ASSERT_FALSE(k.solar.has_value());
}

void test_parse_empty_events() {
    Kitchen k;
    std::string err;
    const char* j = R"({"calendar":{"today":{"date":"a","events":[]},"tomorrow":{"date":"b","events":[]}},"solar":null,"server_time":"2026-11-10T22:05:00+01:00"})";
    TEST_ASSERT_TRUE(parse(j, k, err));
    TEST_ASSERT_EQUAL(0, (int)k.today.events.size());
    TEST_ASSERT_EQUAL(0, (int)k.tomorrow.events.size());
}

void test_parse_invalid_json() {
    Kitchen k;
    std::string err;
    TEST_ASSERT_FALSE(parse("{not json", k, err));
    TEST_ASSERT_FALSE(err.empty());
}

void test_parse_missing_calendar() {
    Kitchen k;
    std::string err;
    TEST_ASSERT_FALSE(parse(R"({"solar":null})", k, err));
    TEST_ASSERT_FALSE(err.empty());
}

void test_parse_umlaut_summary() {
    Kitchen k;
    std::string err;
    const char* j = "{\"calendar\":{\"today\":{\"date\":\"a\",\"events\":[{\"summary\":\"Käse & Müsli\",\"start\":\"x\",\"end\":\"y\",\"allDay\":false}]},\"tomorrow\":{\"date\":\"b\",\"events\":[]}}}";
    TEST_ASSERT_TRUE(parse(j, k, err));
    TEST_ASSERT_EQUAL_STRING("Käse & Müsli", k.today.events[0].summary.c_str());
}

static std::string withHistory(const std::string& history) {
    return std::string(R"({"calendar":{"today":{"date":"a","events":[]},"tomorrow":{"date":"b","events":[]}},"solar":null,"history":)") +
           history + R"(,"server_time":"2026-11-10T22:05:00+01:00"})";
}

static const char* HIST_OK =
    R"({"date":"2026-11-26","start":"06:00","step_min":5,"unit_w":100,"pv":[12,null],"load":[9,3],"pv_wh":40460,"load_wh":11800,"updated_at":"2026-11-26T19:30:00Z"})";

void test_parse_history_valid() {
    Kitchen k;
    std::string err, j = withHistory(HIST_OK);
    TEST_ASSERT_TRUE(parse(j.c_str(), k, err));
    TEST_ASSERT_TRUE(k.history.has_value());
    const History& h = *k.history;
    TEST_ASSERT_EQUAL_STRING("2026-11-26", h.date.c_str());
    TEST_ASSERT_EQUAL(360, h.startMin);
    TEST_ASSERT_EQUAL(5, h.stepMin);
    TEST_ASSERT_EQUAL(100, h.unitW);
    TEST_ASSERT_EQUAL(2, (int)h.pv.size());
    TEST_ASSERT_EQUAL(12, h.pv[0]);
    TEST_ASSERT_EQUAL(kNoData, h.pv[1]);
    TEST_ASSERT_EQUAL(9, h.load[0]);
    TEST_ASSERT_EQUAL(3, h.load[1]);
    TEST_ASSERT_EQUAL(40460, h.pvWh);
    TEST_ASSERT_EQUAL(11800, h.loadWh);
}

void test_parse_history_absent() {
    Kitchen k;
    std::string err;
    TEST_ASSERT_TRUE(parse(FULL, k, err));
    TEST_ASSERT_FALSE(k.history.has_value());
}

void test_parse_history_malformed_is_dropped() {
    std::string bins193 = "[";
    for (int i = 0; i < 193; i++) bins193 += i ? ",1" : "1";
    bins193 += "]";
    const std::string cases[] = {
        R"({"date":"2026-11-26","start":"06:00","step_min":5,"unit_w":100,"pv":["x"],"load":[1],"pv_wh":0,"load_wh":0})",
        R"({"date":"2026-11-26","start":"06:00","step_min":5,"unit_w":100,"pv":[1,2],"load":[1],"pv_wh":0,"load_wh":0})",
        R"({"date":"2026-11-26","start":"06:00","step_min":5,"unit_w":100,"pv":)" + bins193 + R"(,"load":)" + bins193 + R"(,"pv_wh":0,"load_wh":0})",
        R"({"date":"2026-11-26","start":"06:00","step_min":0,"unit_w":100,"pv":[1],"load":[1],"pv_wh":0,"load_wh":0})",
        R"({"date":"2026-11-26","start":"6","step_min":5,"unit_w":100,"pv":[1],"load":[1],"pv_wh":0,"load_wh":0})",
        R"({"date":"2026-11-26","start":"06:00","step_min":5,"unit_w":100,"pv":[-1],"load":[1],"pv_wh":0,"load_wh":0})",
        R"({"date":"2026-11-26","start":"06:00","step_min":5,"unit_w":0,"pv":[1],"load":[1],"pv_wh":0,"load_wh":0})",
        R"({"start":"06:00","step_min":5,"unit_w":100,"pv":[1],"load":[1],"pv_wh":0,"load_wh":0})",
        R"("nope")",
    };
    for (const std::string& c : cases) {
        Kitchen k;
        std::string err, j = withHistory(c);
        TEST_ASSERT_TRUE_MESSAGE(parse(j.c_str(), k, err), c.c_str());
        TEST_ASSERT_FALSE_MESSAGE(k.history.has_value(), c.c_str());
    }
}

void test_battery_curve() {
    TEST_ASSERT_EQUAL(100, batteryPercent(4200));
    TEST_ASSERT_EQUAL(63, batteryPercent(3800));
    TEST_ASSERT_EQUAL(48, batteryPercent(3700));
    TEST_ASSERT_EQUAL(0, batteryPercent(3300));
    TEST_ASSERT_EQUAL(0, batteryPercent(0));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parse_full);
    RUN_TEST(test_parse_solar_null);
    RUN_TEST(test_parse_empty_events);
    RUN_TEST(test_parse_invalid_json);
    RUN_TEST(test_parse_missing_calendar);
    RUN_TEST(test_parse_umlaut_summary);
    RUN_TEST(test_battery_curve);
    RUN_TEST(test_parse_history_valid);
    RUN_TEST(test_parse_history_absent);
    RUN_TEST(test_parse_history_malformed_is_dropped);
    return UNITY_END();
}

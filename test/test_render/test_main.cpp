#include <unity.h>

#include <vector>

#include "render.h"

void setUp() {}
void tearDown() {}

static View dayView() {
    View v;
    v.screen = Screen::Day;
    v.headerDate = "Di 10. Nov";
    v.pvText = "2.4 kW";
    v.houseText = "Haus 0.9 kW";
    v.gridText = "Netz → 1.1 kW";
    v.batterySoc = 78;
    v.todayLabel = "Heute";
    v.tomorrowLabel = "Morgen";
    v.today = {{"08:00", "Fiete KiTa"}};
    v.tomorrow = {{"ganztägig", "Müllabfuhr"}};
    v.updatedAt = "06:40";
    return v;
}

static int count(const Canvas& c, Color col) {
    int n = 0;
    for (int y = 0; y < Canvas::H; y++)
        for (int x = 0; x < Canvas::W; x++)
            if (c.get(x, y) == col) n++;
    return n;
}

void test_day_banner_yellow() {
    static uint8_t buf[Canvas::kBytes];
    Canvas c(buf);
    renderDay(c, dayView());
    TEST_ASSERT_EQUAL(static_cast<int>(Color::Yellow), static_cast<int>(c.get(5, 5)));
}

void test_day_panel_warning_red() {
    static uint8_t buf[Canvas::kBytes];
    Canvas c(buf);
    View v = dayView();
    v.warnPanelBattery = true;
    renderDay(c, v);
    TEST_ASSERT_EQUAL(static_cast<int>(Color::Red), static_cast<int>(c.get(5, 5)));
}

void test_night_no_yellow_pv() {
    static uint8_t buf[Canvas::kBytes];
    Canvas c(buf);
    View v = dayView();
    v.screen = Screen::Night;
    renderNight(c, v);
    TEST_ASSERT_TRUE(count(c, Color::Yellow) >= 50);
    TEST_ASSERT_TRUE(count(c, Color::Black) > 100);
}

void test_overflow() {
    View v = dayView();
    v.today.clear();
    for (int i = 0; i < 8; i++) v.today.push_back({"09:00", "Termin"});
    TEST_ASSERT_EQUAL(4, visibleTodayLines(v));  // 4 events + "+4 weitere"
    v.today.resize(5);
    TEST_ASSERT_EQUAL(5, visibleTodayLines(v));
    v.today.resize(2);
    TEST_ASSERT_EQUAL(2, visibleTodayLines(v));
}

void test_charge_me_mostly_red_text() {
    static uint8_t buf[Canvas::kBytes];
    Canvas c(buf);
    renderChargeMe(c);
    int red = count(c, Color::Red), white = count(c, Color::White);
    TEST_ASSERT_TRUE(red > 100);
    TEST_ASSERT_EQUAL(Canvas::W * Canvas::H, red + white);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_day_banner_yellow);
    RUN_TEST(test_day_panel_warning_red);
    RUN_TEST(test_night_no_yellow_pv);
    RUN_TEST(test_overflow);
    RUN_TEST(test_charge_me_mostly_red_text);
    return UNITY_END();
}

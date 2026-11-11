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

// Lowest row with black pixels left of the footer timestamp (x < 300).
static int lowestInk(const Canvas& c) {
    for (int y = Canvas::H - 1; y >= 0; y--)
        for (int x = 0; x < 300; x++)
            if (c.get(x, y) == Color::Black) return y;
    return -1;
}

void test_tomorrow_events_stack_vertically() {
    static uint8_t buf[Canvas::kBytes];
    Canvas c(buf);
    View one = dayView();
    one.tomorrow = {{"07:00", "KiTa geschlossen"}};
    renderDay(c, one);
    int y1 = lowestInk(c);
    View two = one;
    two.tomorrow.push_back({"13:30", "Putzfrau"});
    renderDay(c, two);
    int y2 = lowestInk(c);
    TEST_ASSERT_GREATER_OR_EQUAL(12, y2 - y1);  // second event on its own line
    TEST_ASSERT_LESS_THAN(286, y2);             // stays clear of the footer
}

void test_tomorrow_overflow_stays_above_footer() {
    static uint8_t buf[Canvas::kBytes];
    Canvas c(buf);
    View v = dayView();
    v.today = {{"08:00", "a"}, {"09:00", "b"}, {"10:00", "c"}, {"11:00", "d"}, {"12:00", "e"}};
    for (int i = 0; i < 8; i++) v.tomorrow.push_back({"09:00", "Termin"});
    renderDay(c, v);
    TEST_ASSERT_LESS_THAN(286, lowestInk(c));
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
    // banner: y 0..91; calendar below
    int bannerBlack = 0, bannerYellow = 0, belowYellow = 0, catBlack = 0;
    for (int y = 0; y < Canvas::H; y++)
        for (int x = 0; x < Canvas::W; x++) {
            Color p = c.get(x, y);
            if (y < 92) {
                bannerBlack += p == Color::Black;
                bannerYellow += p == Color::Yellow;
                catBlack += p == Color::Black && x >= 320 && y < 89;
            } else {
                belowYellow += p == Color::Yellow;
            }
        }
    TEST_ASSERT_TRUE(bannerYellow > 1000);
    TEST_ASSERT_TRUE(bannerBlack > 500);   // text, cat and border
    TEST_ASSERT_TRUE(catBlack >= 50);      // cat on the right
    TEST_ASSERT_EQUAL(0, belowYellow);
    TEST_ASSERT_EQUAL(static_cast<int>(Color::Black), static_cast<int>(c.get(5, 90)));
}

void test_night_panel_warning_keeps_banner() {
    static uint8_t buf[Canvas::kBytes];
    Canvas c(buf);
    View v = dayView();
    v.warnPanelBattery = true;
    renderNight(c, v);
    TEST_ASSERT_EQUAL(static_cast<int>(Color::Red), static_cast<int>(c.get(5, 5)));
    TEST_ASSERT_EQUAL(static_cast<int>(Color::Yellow), static_cast<int>(c.get(5, 30)));
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
    RUN_TEST(test_night_panel_warning_keeps_banner);
    RUN_TEST(test_overflow);
    RUN_TEST(test_charge_me_mostly_red_text);
    RUN_TEST(test_tomorrow_events_stack_vertically);
    RUN_TEST(test_tomorrow_overflow_stays_above_footer);
    return UNITY_END();
}

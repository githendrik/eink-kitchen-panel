#include "view.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace {
constexpr int64_t kSolarStaleSec = 30 * 60;
constexpr int64_t kDataStaleSec = 45 * 60;
const char* kWd[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
const char* kMon[] = {"Jan", "Feb", "Mär", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dez"};

tm localAt(int64_t t, const char* tz) {
    setenv("TZ", tz, 1);
    tzset();
    time_t tt = static_cast<time_t>(t);
    tm lt{};
    localtime_r(&tt, &lt);
    return lt;
}

std::string hhmm(int64_t t, const char* tz) {
    tm lt = localAt(t, tz);
    char b[8];
    snprintf(b, sizeof b, "%02d:%02d", lt.tm_hour, lt.tm_min);
    return b;
}

std::string kw(int w) {
    int tenths = (std::abs(w) + 50) / 100;
    char b[24];
    snprintf(b, sizeof b, "%d.%d kW", tenths / 10, tenths % 10);
    return b;
}

// Weekday 0=Sunday for a civil date (Sakamoto).
int weekday(int y, int m, int d) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

std::string headerDate(const std::string& date) {
    int y, m, d;
    if (sscanf(date.c_str(), "%d-%d-%d", &y, &m, &d) != 3 || m < 1 || m > 12) return date;
    char b[32];
    snprintf(b, sizeof b, "%s %d. %s", kWd[weekday(y, m, d)], d, kMon[m - 1]);
    return b;
}

std::vector<EventLine> lines(const Day& day) {
    std::vector<EventLine> allDay, timed;
    for (const Event& e : day.events) {
        if (e.allDay) {
            allDay.push_back({"ganztägig", e.summary});
        } else {
            timed.push_back({e.start.size() >= 16 ? e.start.substr(11, 5) : e.start, e.summary});
        }
    }
    allDay.insert(allDay.end(), timed.begin(), timed.end());
    return allDay;
}

// "40 kWh", or one decimal below 10 kWh ("3.2 kWh").
std::string kwh(int wh) {
    char b[16];
    long tenths = std::lround(wh / 100.0);
    if (tenths < 100)
        snprintf(b, sizeof b, "%ld.%ld kWh", tenths / 10, tenths % 10);
    else
        snprintf(b, sizeof b, "%ld kWh", std::lround(wh / 1000.0));
    return b;
}

ChartView chartView(const History& h, const tm& lt) {
    ChartView c;
    c.present = true;
    c.startMin = h.startMin;
    c.stepMin = h.stepMin;
    c.nowMin = lt.tm_hour * 60 + lt.tm_min;
    int peak = 0;
    auto watts = [&](const std::vector<int16_t>& in, std::vector<int>& out) {
        for (int16_t u : in) {
            int w = u == kNoData ? -1 : u * h.unitW;
            out.push_back(w);
            if (w > peak) peak = w;
        }
    };
    watts(h.pv, c.pvW);
    watts(h.load, c.loadW);
    c.scaleKw = peak <= 0 ? 1 : (peak + 999) / 1000;
    c.scaleText = std::to_string(c.scaleKw) + " kW";
    c.totalsText = "Erzeugt " + kwh(h.pvWh) + " · Verbraucht " + kwh(h.loadWh);
    return c;
}

View build(const Kitchen& k, int64_t nowUtc, const char* tz, int panelPct, int64_t lastSuccessUtc, bool night) {
    View v;
    v.screen = night ? Screen::Night : Screen::Day;
    v.headerDate = headerDate(k.today.date);
    tm lt = localAt(nowUtc, tz);
    char today[16];
    snprintf(today, sizeof today, "%04d-%02d-%02d", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
    bool shifted = k.today.date != today;
    v.todayLabel = shifted ? "Morgen" : "Heute";
    v.tomorrowLabel = shifted ? "Übermorgen" : "Morgen";
    v.today = lines(k.today);
    v.tomorrow = lines(k.tomorrow);
    v.updatedAt = hhmm(nowUtc, tz);
    v.warnPanelBattery = panelPct < 20;

    if (!k.solar) {
        v.solarMissing = true;
    } else {
        const Solar& s = *k.solar;
        v.pvW = s.pvW;
        v.loadW = s.loadW;
        v.gridW = s.gridW;
        v.batterySoc = s.batterySoc;
        v.pvText = kw(s.pvW);
        v.houseText = "Haus " + kw(s.loadW);
        v.gridText = std::string(s.gridW < 0 ? "Netz → " : "Netz ← ") + kw(s.gridW);
        v.batteryArrow = s.batteryW >= 50 ? "↑" : (s.batteryW <= -50 ? "↓" : "");
        v.solarStale = s.stale || nowUtc - s.updatedAtUtc > kSolarStaleSec;
        if (v.solarStale) v.solarStandText = "Stand " + hhmm(s.updatedAtUtc, tz);
        v.warnGridImport = s.gridW > 50;
        v.warnHomeBatteryLow = s.batterySoc < 15;
    }

    if (!night && lastSuccessUtc > 0 && nowUtc - lastSuccessUtc > kDataStaleSec) {
        v.dataStale = true;
        v.staleText = "Stand " + hhmm(lastSuccessUtc, tz);
    }
    if (!night && k.history && k.history->date == today) v.chart = chartView(*k.history, lt);
    return v;
}

void mix(uint32_t& h, const std::string& s) {
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    h ^= 0xFF;
    h *= 16777619u;
}
void mix(uint32_t& h, long n) { mix(h, std::to_string(n)); }

int roundDiv(int v, int d) {
    int a = std::abs(v);
    int r = (a + d / 2) / d;
    return v < 0 ? -r : r;
}
}  // namespace

View buildDayView(const Kitchen& k, int64_t nowUtc, const char* tz, int panelBatteryPct, int64_t lastSuccessUtc) {
    return build(k, nowUtc, tz, panelBatteryPct, lastSuccessUtc, false);
}

View buildNightView(const Kitchen& k, int64_t nowUtc, const char* tz, int panelBatteryPct) {
    return build(k, nowUtc, tz, panelBatteryPct, 0, true);
}

uint32_t fingerprint(const View& v) {
    uint32_t h = 2166136261u;
    mix(h, static_cast<long>(v.screen));
    mix(h, v.headerDate);
    mix(h, roundDiv(v.pvW, 100));
    mix(h, roundDiv(v.loadW, 100));
    mix(h, roundDiv(v.gridW, 100));
    mix(h, v.gridW < 0 ? 0 : 1);
    mix(h, v.batterySoc / 5);
    mix(h, v.batteryArrow);
    mix(h, v.solarMissing);
    mix(h, v.solarStale);
    mix(h, v.warnGridImport);
    mix(h, v.warnHomeBatteryLow);
    mix(h, v.warnPanelBattery);
    mix(h, v.dataStale);
    mix(h, v.todayLabel);
    mix(h, v.tomorrowLabel);
    for (const auto* list : {&v.today, &v.tomorrow}) {
        mix(h, static_cast<long>(list->size()));
        for (const EventLine& e : *list) {
            mix(h, e.time);
            mix(h, e.title);
        }
    }
    mix(h, v.chart.present);
    if (v.chart.present) {
        mix(h, v.chart.totalsText);
        mix(h, v.chart.scaleKw);
        for (const auto* bins : {&v.chart.pvW, &v.chart.loadW}) {
            mix(h, static_cast<long>(bins->size()));
            for (int w : *bins) mix(h, w < 0 ? -1L : roundDiv(w, 100));
        }
    }
    return h;
}

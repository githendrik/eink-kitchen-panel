#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "kitchen.h"

enum class Screen { Day, Night, ChargeMe, Maintenance };

struct EventLine {
    std::string time;  // "08:00" or "ganztägig"
    std::string title;
};

struct View {
    Screen screen = Screen::Day;
    std::string headerDate;  // "Di 10. Nov"
    std::string pvText;      // "2.4 kW"
    std::string houseText, gridText;
    int pvW = 0, loadW = 0, gridW = 0;  // raw values, hashed after rounding
    int batterySoc = 0;
    std::string batteryArrow;  // "↑" charging, "↓" discharging, "" idle
    bool solarMissing = false, solarStale = false;
    std::string solarStandText;  // "Stand 07:15"
    bool warnGridImport = false, warnHomeBatteryLow = false, warnPanelBattery = false, dataStale = false;
    std::string staleText;  // "Stand 07:15" when dataStale
    std::string todayLabel, tomorrowLabel;
    std::vector<EventLine> today, tomorrow;
    std::string updatedAt;  // "06:40", excluded from fingerprint
};

// lastSuccessUtc == 0 means "never fetched": dataStale stays false.
View buildDayView(const Kitchen& k, int64_t nowUtc, const char* tz, int panelBatteryPct, int64_t lastSuccessUtc);
View buildNightView(const Kitchen& k, int64_t nowUtc, const char* tz, int panelBatteryPct);

// FNV-1a over rounded values, flags and texts; excludes updatedAt and clock times.
uint32_t fingerprint(const View& v);

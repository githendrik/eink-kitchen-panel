#pragma once
#include <cstdint>

#include "schedule.h"

enum class WakeAction { ChargeMe, Maintenance, Refresh };

struct WakeInput {
    int batteryPct = 100;
    bool confirm = false;
    bool down = false;
    bool settingsComplete = true;  // WiFi credentials and backend URL/token present
    bool timeKnown = true;         // RTC holds a plausible time
    Phase phase = Phase::Day;      // only meaningful when timeKnown
};

constexpr int kBatteryCriticalPct = 5;
constexpr int64_t kMinDaySleepSeconds = 60;
constexpr int64_t kStaleAfterSeconds = 45 * 60;

WakeAction decideWake(const WakeInput& in);

// A day plan that would sleep under a minute: render the night screen now instead.
bool dayPlanTooShort(const Plan& p);

struct WakePlan {
    bool renderNight;  // night screen (calendar + cat) instead of the day screen
    int shift;         // /kitchen?shift= value to fetch
    int64_t sleepSeconds;
};

// Single source of truth for what to show and how long to sleep. Night sleeps shorter than
// 60 s (clock drift just before 06:00) are treated as day: live day screen, sleep just past 06:00.
WakePlan planWake(int64_t nowUtc, const char* tz, bool downPressed);

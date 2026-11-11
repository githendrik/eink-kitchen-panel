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

// server_time earlier than 2026-01-01T00:00:00Z means missing/invalid (parse yields 0).
constexpr int64_t kMinPlausibleServerUtc = 1767225600;
bool serverTimePlausible(int64_t utc);

constexpr int64_t kChargeMeSleepSeconds = 15 * 60;

struct ChargeMePlan {
    bool draw;             // false if the ChargeMe screen is already shown
    int64_t sleepSeconds;  // 0 = no timer
    bool wakeOnCharger;
};

// While the charger is connected CHRG_L stays low, so a charger wake bit would re-wake at once:
// sleep on a timer instead and re-check the battery.
ChargeMePlan planChargeMe(bool charging, bool chargerWake, bool alreadyShown);

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

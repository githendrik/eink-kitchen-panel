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

// True when a Refresh wake should render the night screen (night phase, no Down press).
bool wantsNightScreen(const WakeInput& in);

// A day plan that would sleep under a minute: render the night screen now instead.
bool dayPlanTooShort(const Plan& p);

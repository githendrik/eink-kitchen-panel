#include "wake.h"

WakeAction decideWake(const WakeInput& in) {
    if (in.batteryPct < kBatteryCriticalPct) return WakeAction::ChargeMe;
    if (in.confirm || !in.settingsComplete) return WakeAction::Maintenance;
    return WakeAction::Refresh;
}

bool wantsNightScreen(const WakeInput& in) {
    return in.timeKnown && in.phase == Phase::Night && !in.down;
}

bool dayPlanTooShort(const Plan& p) { return p.sleepSeconds < kMinDaySleepSeconds; }

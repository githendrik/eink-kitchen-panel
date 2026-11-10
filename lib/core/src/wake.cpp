#include "wake.h"

WakeAction decideWake(const WakeInput& in) {
    if (in.batteryPct < kBatteryCriticalPct) return WakeAction::ChargeMe;
    if (in.confirm || !in.settingsComplete) return WakeAction::Maintenance;
    return WakeAction::Refresh;
}

bool dayPlanTooShort(const Plan& p) { return p.sleepSeconds < kMinDaySleepSeconds; }

WakePlan planWake(int64_t nowUtc, const char* tz, bool downPressed) {
    if (phaseAt(nowUtc, tz) == Phase::Night) {
        if (downPressed) return WakePlan{false, 0, 600};
        Plan n = planNight(nowUtc, tz);
        if (n.sleepSeconds >= kMinDaySleepSeconds) return WakePlan{true, n.shift, n.sleepSeconds};
        return WakePlan{false, 0, n.sleepSeconds + 5};
    }
    Plan p = planAfterDayRefresh(nowUtc, tz);
    if (dayPlanTooShort(p)) return WakePlan{true, 1, planNight(nowUtc, tz).sleepSeconds};
    return WakePlan{false, 0, p.sleepSeconds};
}

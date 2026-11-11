#include "wake.h"

WakeAction decideWake(const WakeInput& in) {
    if (in.downHeld) return WakeAction::Off;
    if (in.batteryPct < kBatteryCriticalPct) return WakeAction::ChargeMe;
    if (in.confirm || !in.settingsComplete) return WakeAction::Maintenance;
    return WakeAction::Refresh;
}

bool serverTimePlausible(int64_t utc) { return utc >= kMinPlausibleServerUtc; }

ChargeMePlan planChargeMe(bool charging, bool chargerWake, bool alreadyShown) {
    if (charging || chargerWake) return ChargeMePlan{!alreadyShown, kChargeMeSleepSeconds, false};
    return ChargeMePlan{!alreadyShown, 0, true};
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

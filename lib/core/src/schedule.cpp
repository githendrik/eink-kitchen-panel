#include "schedule.h"

#include <cstdlib>
#include <ctime>

namespace {
constexpr int kDayStart = 6;
constexpr int kDayEnd = 22;
constexpr int64_t kInterval = 900;

void applyTz(const char* tz) {
    setenv("TZ", tz, 1);
    tzset();
}

tm localAt(int64_t t) {
    time_t tt = static_cast<time_t>(t);
    tm lt{};
    localtime_r(&tt, &lt);
    return lt;
}

// Epoch of the next local `hour`:00:00 strictly after `now`.
int64_t nextLocal(int64_t now, int hour) {
    tm lt = localAt(now);
    bool past = lt.tm_hour >= hour;
    lt.tm_hour = hour;
    lt.tm_min = 0;
    lt.tm_sec = 0;
    if (past) lt.tm_mday += 1;
    lt.tm_isdst = -1;
    return static_cast<int64_t>(mktime(&lt));
}
}  // namespace

Phase phaseAt(int64_t nowUtc, const char* tz) {
    applyTz(tz);
    int h = localAt(nowUtc).tm_hour;
    return (h >= kDayStart && h < kDayEnd) ? Phase::Day : Phase::Night;
}

Plan planNight(int64_t nowUtc, const char* tz) {
    applyTz(tz);
    int shift = localAt(nowUtc).tm_hour >= kDayEnd ? 1 : 0;
    return {Phase::Night, shift, nextLocal(nowUtc, kDayStart) - nowUtc};
}

Plan planAfterDayRefresh(int64_t nowUtc, const char* tz) {
    applyTz(tz);
    if (localAt(nowUtc).tm_hour >= kDayEnd || localAt(nowUtc).tm_hour < kDayStart) {
        return planNight(nowUtc, tz);
    }
    int64_t toEnd = nextLocal(nowUtc, kDayEnd) - nowUtc;
    return {Phase::Day, 0, toEnd < kInterval ? toEnd : kInterval};
}

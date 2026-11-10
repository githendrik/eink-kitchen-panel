#pragma once
#include <cstdint>

enum class Phase { Day, Night };
struct Plan {
    Phase phase;
    int shift;
    int64_t sleepSeconds;
};

// tz: POSIX TZ string. Day window is 06:00-22:00 local.
Plan planAfterDayRefresh(int64_t nowUtc, const char* tz);
Plan planNight(int64_t nowUtc, const char* tz);
Phase phaseAt(int64_t nowUtc, const char* tz);

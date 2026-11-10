#include "battery.h"

int batteryPercent(int millivolts) {
    long long v = millivolts;
    long long p = (-v * v + 9016 * v - 19189000) / 10000;
    if (p < 0) return 0;
    if (p > 100) return 100;
    return static_cast<int>(p);
}

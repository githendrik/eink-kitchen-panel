#pragma once
#include <stdint.h>

namespace board {
void begin();
enum class Wake { PowerOn, Timer, ButtonConfirm, ButtonDown, Charger, Other };
Wake wakeCause();
int batteryMillivolts();
bool charging();
bool upPressed();
void led(bool on);  // green status LED (active low)
[[noreturn]] void deepSleep(int64_t seconds, bool wakeOnCharger);
}  // namespace board

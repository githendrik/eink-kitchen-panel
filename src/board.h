#pragma once
#include <stdint.h>

namespace board {
void begin();
enum class Wake { PowerOn, Timer, ButtonConfirm, ButtonDown, Charger, Other };
Wake wakeCause();
int batteryMillivolts();
bool charging();
bool upPressed();
[[noreturn]] void deepSleep(int64_t seconds, bool wakeOnCharger);
}  // namespace board

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
bool downHeldFor(uint32_t ms);  // true if Down stays pressed for the whole period
[[noreturn]] void deepSleep(int64_t seconds, bool wakeOnCharger);
[[noreturn]] void powerOff();   // release the power latch; button-only wake on USB
}  // namespace board

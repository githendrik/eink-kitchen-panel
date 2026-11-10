#pragma once

// One wake cycle; always ends in board::deepSleep.
[[noreturn]] void runWakeCycle();

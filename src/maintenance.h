#pragma once

// Maintenance mode: settings portal, update check/install. Never returns: ends in esp_restart()
// (configured) or button-only deep sleep (unconfigured, to spare the battery).
[[noreturn]] void runMaintenance();

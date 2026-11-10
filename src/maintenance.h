#pragma once

// Stub until Task 8. Renders the maintenance screen only when entered via Confirm or PowerOn,
// then deep-sleeps with no timer (button-only wake) so an unconfigured panel does not drain
// the battery with periodic refreshes.
[[noreturn]] void runMaintenance();

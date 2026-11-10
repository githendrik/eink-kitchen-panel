# Kitchen panel firmware — design

Date: 2026-11-10 · Status: draft for review · Companion: `home-apis/docs/superpowers/specs/2026-11-10-home-apis-design.md`

## Intent

A battery-powered ZECTRIX NOTE4C devkit on the kitchen counter that answers, at
a glance: **how much is our PV producing right now, and what's on today?**
Second glance: tomorrow's events and the home (GoodWe) battery state. Battery
life is the main design driver: the panel sleeps most of the time.

## Hardware

ZECTRIX NOTE4C devkit: ESP32-S3, 16 MB flash, 8 MB PSRAM, 4.2" 400×300
**four-colour BWRY** (black/white/red/yellow) e-paper, built-in ~2000 mAh battery,
USB-C, physical buttons. Docs: https://wiki.zectrix.com (NOTE4 dev guide, spec page).

## Goals

- Show layout A (below), fed by one `GET /kitchen` call to home-apis.
- Refresh at most every **15 min**, only between **06:00 and 22:00** local time.
- Maximise battery life (deep sleep between wakes, skip redraws when nothing changed).
- Never let a dead/stale panel look current.
- OTA updates and settings, reachable on demand via a button.

## Non-goals

- Talking to iCloud or GoodWe directly.
- Touch/interactive UI beyond buttons. Graphs/history.

## Anti-goals

- Draining the battery with frequent wake-ups or redraws.
- A screen that silently shows old data as if live.
- A dead panel that looks alive (e-ink retains the image).

## Decisions (by the user)

- **Layout A**: yellow solar banner on top (big PV kW; house, grid direction,
  home-battery % with bar), today's events large below, tomorrow condensed
  at the bottom, small "updated at" time.
- **Red is reserved for warnings**: panel battery low, importing from grid, home
  battery nearly empty, solar data stale. (The mockup's red "imminent event"
  highlight is dropped — assistant's call: it would need time-based redraws.)
- **Night (22:00–06:00)**: at 22:00 a final redraw — calendar stays but is
  **shifted one day forward** (tomorrow in the big slot, day-after condensed);
  the solar banner is replaced by a "sleeping" banner with a small sleeping-cat
  pixel art. No timed wakes until 06:00. Only a button press wakes it.
- **Maintenance mode** via button: panel wakes, shows a status screen (IP,
  firmware version, battery voltage/%, last fetch), runs the settings web page
  and an OTA check, stays awake until the button is pressed again or 10 min idle.
  The screen is not redrawn repeatedly while in this mode.
- **Physical OTA trigger** button, like the desk panels.
- Low panel battery shown as a red warning bar; a final "battery empty — please
  charge" screen drawn just before the panel would brown out.

Mockup: `.superpowers/brainstorm/*/content/layout.html` — card A (layout) and
the fourth card "Warnings" (red warning bar + red values), which applies on top of A.

## Behaviour

### Wake cycle (06:00–22:00)

1. Wake from timer or button. Determine wake cause.
2. Read battery. If below critical threshold → draw "charge me" screen, deep sleep
   with no timer (button only).
3. Connect WiFi (stored creds), `GET /kitchen` with bearer token. Use
   `server_time` from the response to keep the RTC honest; NTP only if needed.
4. Compute a "visible state" fingerprint from what would be drawn, using
   **rounded** solar values (e.g. PV to 0.1 kW, SoC to 5 %, grid direction) plus
   calendar content and warning flags. If equal to the last fingerprint (kept in
   RTC memory) → skip redraw. The "updated at" time is excluded from the
   fingerprint.
5. Otherwise full refresh of layout A.
6. Compute next wake: now + 15 min, or 22:00 if sooner (night transition), and
   deep sleep.

On fetch failure: keep the last screen; if the displayed data is older than a
threshold (e.g. 45 min) redraw with a visible "last update HH:MM" stale marker.
Retry on the next normal wake (no rapid retries on battery).

Server-side stale solar (`stale: true`, or `updated_at` older than 30 min) or
`solar: null` (assistant's call): the banner shows the last values with a red
"Stand HH:MM" marker, or "Solar: keine Daten" when null. Staleness is part of the
fingerprint, so the marker appears on the next wake.

### Night transition (22:00)

Fetch `/kitchen?shift=1`, draw the night screen, deep sleep until 06:00 with
button wake enabled.

Button press at night (assistant's call): draw the normal live screen, sleep a
single 10-minute timer, then redraw the night screen and sleep until 06:00
(button-only). Shift rule: `shift=1` only between 22:00 and 24:00; after midnight
the calendar date is already correct, so `shift=0` for both screens.

### Battery empty / recovery

After the "charge me" screen, wake on button **or** on USB power appearing, if
the NOTE4C exposes a charge/VBUS-detect signal on an RTC GPIO (open item). If it
doesn't, a button press is required after charging and the "charge me" screen
says so ("Nach dem Laden Taste drücken").

### Buttons

Buttons on RTC-capable GPIOs (ESP32-S3: GPIO 0–21) wake the chip from deep sleep
via ext1; the wake cause identifies which button. Proposed mapping (pins to be
confirmed from the NOTE4C schematic):

- Button 1: maintenance mode (settings + status + OTA check).
- Button 2: refresh now.
- Button 3 (if present): OTA check only.

### Settings & OTA (reuse from eink-desk-panel)

- WiFiManager captive portal for WiFi; config (backend URL, bearer token, refresh
  interval, quiet hours, thresholds) in NVS via a ConfigManager like the desk panel's.
- OTA from GitHub Releases exactly as in `eink-desk-panel/src/ota_update.h`
  (A/B partitions, rollback, mark valid after WiFi+fetch). Only in maintenance
  mode or via the OTA button, never on a normal timed wake.
- Remote logging pattern from the desk panel is optional.

## Constraints

- 4-colour BWRY refresh is full-screen and slow (~15–20 s); assume **no partial
  refresh**. Open item: check the controller datasheet for a partial-window mode;
  only adopt it if it doesn't cause ghosting.
- Toolchain: PlatformIO + Arduino on ESP32-S3, matching eink-desk-panel
  (unless the NOTE4C reference firmware forces ESP-IDF; see open items).
- Timezone Europe/Zurich, DST-aware.

## Documentation (user requirement)

Keep `docs/architecture.md` in this repo current as part of every build step:
hardware/pin map, wake/sleep state machine (day, night, maintenance, battery
empty), fetch → fingerprint → render flow, screen layouts, button mapping, NVS
config keys, OTA/release procedure, and power budget notes. Link to
`home-apis/docs/architecture.md` for the backend side. A reader months later
should understand the device without reading code.

## Left to the builder

- Display driver (Zectrix reference vs GxEPD2 if it supports the panel), fonts
  (umlauts needed — event titles are German), exact pixel layout within layout A.
- Battery thresholds after measuring the discharge curve.
- A desktop render simulator like `eink-desk-panel/sim/` is encouraged for layout work.

## Open items

- NOTE4C: display controller model, SPI pins, button pins and count, battery
  ADC pin/divider, any power-gating of the panel/peripherals.
- Partial-refresh support on the controller.
- Label language: **German** (user decision) — all on-screen labels and messages; fonts must cover umlauts/ß.

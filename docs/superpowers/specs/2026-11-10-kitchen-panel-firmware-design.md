# Kitchen panel firmware — design

Date: 2026-11-10 · Status: draft for review · Companion: `home-apis/docs/superpowers/specs/2026-11-10-home-apis-design.md`

## Intent

A battery-powered ZECTRIX NOTE4C devkit on the kitchen counter that answers, at
a glance: **how much is our PV producing right now, and what's on today?**
Second glance: tomorrow's events and the home (GoodWe) battery state. Battery
life is the main design driver: the panel sleeps most of the time.

## Hardware

ZECTRIX NOTE4C devkit: ESP32-S3 N16R8 (16 MB flash, 8 MB PSRAM), 4.2" 400×300
**four-colour BWRY** (black/white/red/yellow) e-paper, built-in ~2000 mAh battery,
USB-C, physical buttons. Docs: https://wiki.zectrix.com (NOTE4 dev guide — the
NOTE4C keeps the NOTE4 mainboard, only the display module differs).

Verified facts (Nov 2026; sources: NOTE4 wiki pin map + the vendor 4-colour
firmware `itopinion/youn-ink-fourcolor-firmware`,
`main/boards/zectrix-s3-epaper-4.2/`). Device on hand: MAC 84:c7:bb:4d:be:ac;
full factory flash backed up to `backup/note4c-factory-84c7bb4dbeac.bin`
(sha256 ac731eaf…4b54, gitignored).

| Function | GPIO | Notes |
|---|---|---|
| **Power latch** | 17 | Must be driven HIGH and held (`gpio_hold_en`) or the board switches off on battery |
| EPD power enable | 6 | HIGH = on; switch off before deep sleep |
| EPD SPI | SCK 12, MOSI 13, CS 11, DC 10, RST 9, BUSY 8 | BUSY **low = busy**; 4-colour refresh can take >10 s (vendor timeout 120 s) |
| Confirm / BOOT button | 0 | active low, RTC-capable → deep-sleep wake |
| Down / Power button | 18 | active low, RTC-capable → deep-sleep wake (also hardware power-on) |
| Up button | 39 | active low, **not** RTC-capable → cannot wake from deep sleep |
| Charging (CHRG_L) | 2 | low = charging, RTC-capable |
| Charge full (STDBY_H) | 1 | high = full |
| Battery ADC | 4 | ADC1_CH3, 12 dB atten, ×2 divider; vendor %: `(-v² + 9016v − 19189000)/10000` (v in mV), clamp 0–100 |
| Status LED | 3 | green, active low |
| Audio power / amp | 42 / 46 | keep off |

Display controller **SSD2683**, 4-colour mode: 2 bits per pixel, MSB-first,
100 bytes per row, 300 rows (30 000 bytes). Colour codes: **black 0, white 1,
yellow 2, red 3**. Sequence: EPD power on → RST high/low(20 ms)/high → wait BUSY
→ cmd `0xE9` data `0x01` → cmd `0x10` + 30 000 bytes → cmd `0x04` (power on),
wait BUSY → cmd `0x12` data `0x00` (refresh), wait BUSY → cmd `0x02` data `0x00`
(power off), wait BUSY → cmd `0x07` data `0xA5` (deep sleep) → EPD power off.
Full refresh only; no partial refresh in 4-colour mode (vendor firmware never
uses one) — settled.

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

After the "charge me" screen, deep sleep waking on a button **or** on CHRG_L
(GPIO2) going low, i.e. as soon as the charger is plugged in. The charge wake is
armed only in this state (it would otherwise re-wake immediately while charging).

### Buttons

Deep-sleep wake via ext1 ANY_LOW on GPIO0 and GPIO18 (GPIO39 cannot wake).
Mapping (assistant's call, from the hardware):

- **Confirm (front, GPIO0)**: maintenance mode (status screen + settings page +
  OTA check). Inside maintenance mode, Confirm again exits.
- **Down (side, GPIO18)**: refresh now (normal fetch + redraw, ignores the
  unchanged-fingerprint skip).
- **Up (side, GPIO39)**: only usable while awake — in maintenance mode it
  triggers "install update now" if one was found.

### Settings & OTA (reuse from eink-desk-panel)

- WiFiManager captive portal for WiFi; config (backend URL, bearer token, refresh
  interval, quiet hours, thresholds) in NVS via a ConfigManager like the desk panel's.
- OTA from GitHub Releases exactly as in `eink-desk-panel/src/ota_update.h`
  (A/B partitions, rollback, mark valid after WiFi+fetch). Only in maintenance
  mode or via the OTA button, never on a normal timed wake.
- Remote logging pattern from the desk panel is optional.

## Constraints

- 4-colour BWRY refresh is full-screen and slow (>10 s); no partial refresh.
- Toolchain: PlatformIO + Arduino on ESP32-S3, matching eink-desk-panel; own
  small SSD2683 driver following the vendor sequence above.
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

- Battery thresholds: start with warning < 20 %, critical < 5 % (vendor curve),
  tune after observing a real discharge.
- Label language: **German** (user decision) — all on-screen labels and messages; fonts must cover umlauts/ß.

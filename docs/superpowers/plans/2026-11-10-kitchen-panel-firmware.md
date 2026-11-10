# Kitchen Panel Firmware Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Battery-powered NOTE4C firmware that wakes every 15 min (06:00–22:00), fetches `/kitchen` from home-apis, renders layout A in black/white/red/yellow, and deep-sleeps; with night screen, maintenance mode, OTA and a low-battery screen.

**Architecture:** All logic that does not touch hardware lives in `lib/core` (plain C++17, no Arduino headers) and is unit-tested on the host with `pio test -e native`; a host simulator renders every screen to PNG for visual checks. A thin Arduino layer (`src/`) does board power, buttons, battery ADC, SSD2683 SPI, WiFi/HTTP, NVS, OTA and deep sleep, and drives the core.

**Tech Stack:** PlatformIO, Arduino-ESP32 (espressif32 platform), ArduinoJson 7 (also used on native), WiFiManager, Unity (pio test). Fonts: Adafruit-GFX `GFXfont` format generated from DejaVu Sans (Latin-1 range), rendered by our own code — no Adafruit_GFX dependency.

**Spec:** `docs/superpowers/specs/2026-11-10-kitchen-panel-firmware-design.md` (backend contract: `~/Repos/github/githendrik/home-apis/docs/architecture.md`; reference firmware patterns: `~/Repos/github/githendrik/eink-desk-panel/src/{ota_update.h,config_manager.h}`)

## Global Constraints

- Display: 400×300, 2 bpp MSB-first, 100 bytes/row, 30 000-byte buffer; colour codes **BLACK=0, WHITE=1, YELLOW=2, RED=3**.
- SSD2683 sequence exactly as in the spec's Hardware section; BUSY low = busy; busy timeout 120 s.
- Pins exactly as in the spec table; power latch GPIO17 HIGH + held from the first line of `setup()`; EPD power GPIO6 off before sleep.
- Day window 06:00–22:00 Europe/Zurich (POSIX TZ `CET-1CEST,M3.5.0,M10.5.0/3`); wake interval 15 min; night screen at 22:00 with `shift=1`; between 00:00 and 06:00 `shift=0`.
- Red only for warnings: panel battery < 20 %, grid import (grid_w > 50), home battery SoC < 15 %, stale/missing solar, stale display data (> 45 min). Solar is stale if `stale==true` or `updated_at` older than 30 min.
- All on-screen text German. Labels: "Heute", "Morgen", "Haus", "Netz", "Akku", "ganztägig", "Stand", "keine Daten", "Panel-Akku schwach – bitte laden", "Akku leer – bitte laden", "Gute Nacht".
- `/kitchen` JSON contract: `{calendar:{today:{label,date,events:[{summary,start,end,allDay}]},tomorrow:{…},fetched_at}, solar:{pv_w,load_w,grid_w,battery_soc,battery_w,updated_at,source,stale}|null, server_time}`; `grid_w` negative = export, `battery_w` positive = charging; timed `start` = `YYYY-MM-DDTHH:MM:SS` local.
- Fingerprint rounding: PV to 100 W, load/grid to 100 W, SoC to 5 %, plus grid direction, warning flags, all event texts, day dates; excludes clock times of fetch.
- Firmware version via build flag `FIRMWARE_VERSION`; OTA source: GitHub releases of `githendrik/eink-kitchen-panel`, asset `firmware.bin`.
- Repo is public: no WiFi passwords or tokens in source.
- `docs/architecture.md` updated in every task that changes behaviour (user requirement).

## Review Focus

- DST switch nights (last Sunday of March/October): next-wake computation around 22:00 and 06:00 must not produce a negative or 25 h sleep → tests in Task 3.
- Event titles with umlauts/ß/emoji and very long titles: umlauts render, unknown glyphs become "?", text is truncated with "…" inside its box → tests in Task 2 and Task 5.
- `/kitchen` with `solar: null`, empty event lists, missing keys or invalid JSON: no crash, sensible screen → tests in Task 3 and Task 5.
- More events than fit (8 today): show as many as fit and "+N weitere" → test in Task 5.
- Backend unreachable for hours: last screen kept, red "Stand HH:MM" stale marker appears once data is > 45 min old, no rapid retry loop → test in Task 4 (decision) and Task 7.

---

### Task 1: Project scaffold, environments, CI

**Files:** Create `platformio.ini`, `partitions.csv`, `src/main.cpp` (blink-free stub: latch GPIO17 high, print version over USB CDC, deep sleep 60 s), `lib/core/library.json`, `test/test_smoke/test_main.cpp`, `.github/workflows/ci.yml`, `docs/architecture.md` (skeleton with the pin table and colour codes from the spec), `AGENTS.md`, update `README.md`.

**Interfaces:** Produces envs `note4c` and `native`.

- [ ] **Step 1:** `platformio.ini`: `[env:note4c]` platform espressif32, board `esp32-s3-devkitc-1`, framework arduino, `board_build.flash_size = 16MB`, `board_upload.flash_size = 16MB`, `board_build.arduino.memory_type = qio_opi`, `board_build.partitions = partitions.csv`, build_flags `-DBOARD_HAS_PSRAM -DARDUINO_USB_CDC_ON_BOOT=1 -DARDUINO_USB_MODE=1 -DFIRMWARE_VERSION='"dev"' -DCONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=1`, lib_deps `bblanchon/ArduinoJson@^7`, `https://github.com/tzapu/WiFiManager.git`, monitor_speed 115200. `[env:native]` platform native, `test_framework = unity`, `build_flags = -std=c++17`, lib_deps ArduinoJson, `test_build_src = no`. Partitions: nvs 0x9000/0x5000, otadata 0xe000/0x2000, app0 0x10000/0x300000, app1 0x310000/0x300000.
- [ ] **Step 2:** `test_smoke`: one Unity test asserting `1 == 1` through a function exported from `lib/core` (`int core_version()` returns 1) to prove the native lib wiring.
- [ ] **Step 3:** Run `pio test -e native` → PASS; `pio run -e note4c` → SUCCESS.
- [ ] **Step 4:** CI: on push and PR run `pio test -e native` and `pio run -e note4c`.
- [ ] **Step 5:** Commit `chore: platformio scaffold, native tests, CI`.

---

### Task 2: Canvas and text rendering (core)

**Files:** Create `lib/core/src/canvas.h/.cpp`, `lib/core/src/text.h/.cpp`, `lib/core/src/fonts/*.h`, `tools/gen_fonts.py`, `tools/fonts/DejaVuSans.ttf`, `tools/fonts/DejaVuSans-Bold.ttf` (+ their LICENSE), `test/test_canvas/test_main.cpp`.

**Interfaces:**
```cpp
enum class Color : uint8_t { Black = 0, White = 1, Yellow = 2, Red = 3 };
class Canvas {
 public:
  static constexpr int W = 400, H = 300, kBytes = 30000;
  explicit Canvas(uint8_t* buf);           // caller owns 30000-byte buffer
  void fill(Color c);
  void pixel(int x, int y, Color c);        // clipped
  Color get(int x, int y) const;
  void fillRect(int x, int y, int w, int h, Color c);
  void rect(int x, int y, int w, int h, Color c);   // 1 px outline
  void hline(int x, int y, int w, Color c);
  const uint8_t* data() const;
};
enum class Font { Small, Body, BodyBold, Title, Huge };  // ~11, 14, 14 bold, 18 bold, 40 bold px
int textWidth(Font f, const char* utf8);
// Draws UTF-8 text with baseline at y; truncates with "…" if wider than maxW (maxW<=0: no limit). Returns drawn width.
int drawText(Canvas& c, int x, int y, Font f, const char* utf8, Color col, int maxW = 0);
int fontLineHeight(Font f);
```

- [ ] **Step 1: failing tests:** `test_fill_white_bytes` (fill White → every byte 0x55); `test_pixel_packing` (pixel(0,0,Red) → byte0 == 0xD5; pixel(3,0,Yellow) on white → byte0 == 0x56; pixel(4,0,Black) → byte1 == 0x15); `test_clip` (pixel(-1,0)/(400,0)/(0,300) no write, no crash); `test_get_roundtrip`; `test_text_umlaut_width` (`textWidth(Body,"Ä") > 0` and equals width of glyph 0xC4); `test_text_unknown_glyph` (`"😀"` renders same width as `"?"`); `test_truncate` (long string with maxW=100 → drawn width ≤ 100 and last glyph is "…" — font must include U+2026 mapped to a spare code, see Step 3); `test_text_draws_pixels` (drawText "Heute" black on white → at least one Black pixel in its box).
- [ ] **Step 2:** `pio test -e native -f test_canvas` → FAIL.
- [ ] **Step 3:** `tools/gen_fonts.py` (Python 3 + Pillow): rasterise DejaVu Sans / Bold at the five sizes, code points 0x20–0x7E, 0xA0–0xFF, plus "…" (U+2026) stored at index for 0x7F, output Adafruit `GFXfont`/`GFXglyph` headers (self-contained struct definitions in `lib/core/src/fonts/gfxfont.h`, bitmaps `static const uint8_t`). Commit generated headers and the script; record the command in `docs/architecture.md`. `text.cpp`: UTF-8 decode → Latin-1 code point (U+2026 → 0x7F, anything else > 0xFF → '?'), draw glyph bitmaps via `Canvas::pixel`.
- [ ] **Step 4:** `pio test -e native -f test_canvas` → PASS; `pio run -e note4c` still builds.
- [ ] **Step 5:** Commit `feat(core): 2bpp canvas and Latin-1 GFX font rendering`.

---

### Task 3: Data parsing, battery and schedule (core)

**Files:** Create `lib/core/src/kitchen.h/.cpp`, `lib/core/src/battery.h/.cpp`, `lib/core/src/schedule.h/.cpp`, `test/test_data/test_main.cpp`, `test/test_schedule/test_main.cpp`.

**Interfaces:**
```cpp
struct Event { std::string summary; std::string start; std::string end; bool allDay; };
struct Day   { std::string date; std::vector<Event> events; };
struct Solar { int pvW, loadW, gridW, batterySoc, batteryW; int64_t updatedAtUtc; bool stale; };
struct Kitchen { Day today, tomorrow; std::optional<Solar> solar; int64_t serverTimeUtc; };
bool parseKitchen(const char* json, size_t len, Kitchen& out, std::string& err); // false on invalid JSON / missing calendar
int  batteryPercent(int millivolts);    // vendor curve, clamped 0..100
enum class Phase { Day, Night };
struct Plan { Phase phase; int shift; int64_t sleepSeconds; };
// localNow: epoch seconds UTC; tz: POSIX TZ string. Day: next wake = min(now+900, today 22:00). Night: until next 06:00.
Plan planAfterDayRefresh(int64_t nowUtc, const char* tz);   // used after a normal day refresh
Plan planNight(int64_t nowUtc, const char* tz);             // shift=1 if local time >= 22:00 else 0; sleep until 06:00
Phase phaseAt(int64_t nowUtc, const char* tz);
```
(Implement local-time math with `setenv("TZ")`/`tzset()`/`localtime_r`/`mktime` — works on native and ESP32.)

- [ ] **Step 1: failing tests:**
  - `test_parse_full`: fixture copied from home-apis docs example (2 events, solar present) → fields equal; `serverTimeUtc` equals epoch of `2026-11-10T22:05:00+01:00`.
  - `test_parse_solar_null` → `!solar`; `test_parse_empty_events`; `test_parse_invalid_json` → false, err non-empty; `test_parse_missing_calendar` → false; `test_parse_umlaut_summary` keeps UTF-8 bytes.
  - `test_battery_curve`: 4200 → 100, 3800 → 63, 3700 → 48, 3300 → 0, 0 → 0.
  - `test_plan_day_normal`: 10:00 local → Day, shift 0, sleep 900.
  - `test_plan_day_before_22`: 21:52 → sleep 480 (to 22:00).
  - `test_plan_night_at_22`: 22:00 → Night, shift 1, sleep 8 h.
  - `test_plan_night_after_midnight`: 00:30 → shift 0, sleep 5.5 h.
  - `test_plan_dst_spring`: 2027-03-27 22:00 local → sleep until 2027-03-28 06:00 CEST = 7 h.
  - `test_plan_dst_autumn`: 2026-10-24 22:00 → 9 h.
  - `test_phase`: 05:59 Night, 06:00 Day, 21:59 Day, 22:00 Night.
- [ ] **Step 2:** `pio test -e native -f test_data -f test_schedule` → FAIL.
- [ ] **Step 3:** Implement (ArduinoJson `JsonDocument`; ISO `updated_at`/`server_time` parsed with `sscanf` + offset handling + `timegm` equivalent).
- [ ] **Step 4:** → PASS. Document schedule rules in `docs/architecture.md`.
- [ ] **Step 5:** Commit `feat(core): kitchen JSON parsing, battery curve, wake schedule`.

---

### Task 4: View model, warnings, fingerprint (core)

**Files:** Create `lib/core/src/view.h/.cpp`, `test/test_view/test_main.cpp`.

**Interfaces:**
```cpp
enum class Screen { Day, Night, ChargeMe, Maintenance };
struct EventLine { std::string time; std::string title; };  // time "08:00" or "ganztägig"
struct View {
  Screen screen;
  std::string headerDate;            // e.g. "Di 10. Nov"
  std::string pvText;                // "2.4 kW" (one decimal, kW)
  std::string houseText, gridText;   // "Haus 0.9 kW", "Netz → 1.1 kW" (export) / "Netz ← 0.6 kW" (import)
  int batterySoc; std::string batteryArrow; // "↑" charging, "↓" discharging, "" idle (|battery_w|<50)
  bool solarMissing, solarStale; std::string solarStandText;   // "Stand 07:15"
  bool warnGridImport, warnHomeBatteryLow, warnPanelBattery, dataStale;
  std::string staleText;             // "Stand 07:15" when dataStale
  std::string todayLabel, tomorrowLabel;   // "Heute"/"Morgen" (night: "Morgen"/"Übermorgen")
  std::vector<EventLine> today, tomorrow;
  std::string updatedAt;             // "06:40" shown bottom-right
};
View buildDayView(const Kitchen& k, int64_t nowUtc, const char* tz, int panelBatteryPct, int64_t lastSuccessUtc);
View buildNightView(const Kitchen& k, int64_t nowUtc, const char* tz, int panelBatteryPct);
uint32_t fingerprint(const View& v);   // FNV-1a over all fields except updatedAt (and solarStandText minutes are included only via solarStale)
```

- [ ] **Step 1: failing tests:** export case (`grid_w=-1100` → "Netz → 1.1 kW", no import warning); import case (`grid_w=600` → warnGridImport, "Netz ← 0.6 kW"); `grid_w=30` → no warning; SoC 12 → warnHomeBatteryLow; panel 15 % → warnPanelBattery; solar null → solarMissing; solar `updated_at` 40 min old → solarStale + "Stand HH:MM"; `lastSuccessUtc` 50 min old → dataStale; all-day → "ganztägig" and listed first; timed start "2026-11-10T08:00:00" → "08:00"; headerDate German weekday/month abbreviations ("Mo Di Mi Do Fr Sa So", "Jan Feb Mär Apr Mai Jun Jul Aug Sep Okt Nov Dez"); night view labels "Morgen"/"Übermorgen" when built before midnight; fingerprint: same data at 10:00 and 10:15 equal; PV 2400→2440 equal; 2400→2500 different; SoC 78→79 equal, 78→81 different; event title change different.
- [ ] **Step 2:** `pio test -e native -f test_view` → FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** → PASS. Document warnings + fingerprint in `docs/architecture.md`.
- [ ] **Step 5:** Commit `feat(core): view model, German labels, warnings and fingerprint`.

---

### Task 5: Screen layouts and host simulator (core + tool)

**Files:** Create `lib/core/src/render.h/.cpp`, `lib/core/src/cat_bitmap.h` (sleeping cat, ~64×40, 2-colour black/yellow), `tools/sim/sim.cpp` + `[env:sim]` (native program, `build_src_filter` to tools/sim), `tools/sim/scenarios/*.json`, `docs/screens/*.png` (generated), `test/test_render/test_main.cpp`.

**Interfaces:**
```cpp
void renderDay(Canvas& c, const View& v);        // layout A + warnings
void renderNight(Canvas& c, const View& v);      // calendar + "Gute Nacht" banner with cat
void renderChargeMe(Canvas& c);                  // big "Akku leer – bitte laden", red
struct MaintInfo { std::string ip, version, ssid, updateText; int batteryMv, batteryPct; std::string lastFetch; };
void renderMaintenance(Canvas& c, const MaintInfo& m);
```
Layout A geometry (from the approved mockup): yellow banner y 0–92 with 3 px black bottom border, PV label + Huge number left, house/grid/battery bar right; today header + up to 5 lines at Body (19 px pitch); tomorrow header + condensed line(s) Small; updated time bottom-right Small. Red bar (24 px) above the banner when `warnPanelBattery`. Overflow: last visible line becomes "+N weitere".

- [ ] **Step 1: failing tests:** `test_day_banner_yellow` (pixel (5,5) Yellow); `test_day_panel_warning_red` (warnPanelBattery → (5,5) Red); `test_night_no_yellow_pv` (night banner contains the cat: at least 50 Yellow pixels, and text "Gute Nacht" region has Black pixels); `test_overflow` (8 events today → renderer reports `"+4 weitere"` via a returned layout struct or by checking a helper `visibleTodayLines(v)==4`); `test_charge_me_mostly_red_text` (some Red pixels, rest White).
- [ ] **Step 2:** `pio test -e native -f test_render` → FAIL.
- [ ] **Step 3:** Implement render + sim. `pio run -e sim && .pio/build/sim/program tools/sim/scenarios docs/screens` writes one PNG per scenario (palette: black #000, white #fff, yellow #f0c000, red #c00000; minimal PNG writer or PPM + `sips`/Pillow conversion script). Scenarios: `day_export`, `day_import_lowbatt`, `day_many_events`, `day_solar_missing`, `day_stale`, `night`, `charge_me`, `maintenance`.
- [ ] **Step 4:** → PASS; open the PNGs and check against the mockup (layout A). Embed them in `docs/architecture.md` (Screens section).
- [ ] **Step 5:** Commit `feat(core): layouts and PNG simulator`.

---

### Task 6: Board and SSD2683 driver (Arduino)

**Files:** Create `src/board.h/.cpp`, `src/epd_ssd2683.h/.cpp`; modify `src/main.cpp`.

**Interfaces:**
```cpp
namespace board {
  void begin();                         // latch GPIO17 HIGH + gpio_hold_en; audio/amp off; LED off
  enum class Wake { PowerOn, Timer, ButtonConfirm, ButtonDown, Charger, Other };
  Wake wakeCause();                     // from esp_sleep_get_wakeup_cause + ext1 status mask
  int  batteryMillivolts();             // ADC1_CH3, 12 dB, curve-fitting calibration, ×2, avg of 10
  bool charging();                      // GPIO2 low
  bool upPressed();                     // GPIO39 low
  [[noreturn]] void deepSleep(int64_t seconds, bool wakeOnCharger); // seconds<=0: no timer; ext1 ANY_LOW on GPIO0|GPIO18 (+GPIO2 if wakeOnCharger); keeps latch held
}
class Epd {
 public:
  void show(const uint8_t* buf30000);   // full sequence from spec, then EPD power off
};
```

- [ ] **Step 1:** Implement; `main.cpp` temporary diagnostic mode compiled with `-DDIAG_TEST_PATTERN`: draws four vertical colour bars + "Ä Ö Ü ß Test" via core, prints battery mV/%, charging, wake cause over serial, then deep-sleeps 120 s.
- [ ] **Step 2:** `pio run -e note4c` → SUCCESS (CI also builds the DIAG variant via a second env `note4c-diag` extending note4c).
- [ ] **Step 3:** Hardware check (controller, device attached at `/dev/cu.usbmodem*`): `pio run -e note4c-diag -t upload`, serial monitor; expected: bars black/white/red/yellow left→right, umlauts readable, plausible battery mV (3300–4250), wake cause logged; press Down during sleep → wakes with ButtonDown. Record observations in `docs/architecture.md` (Hardware verification).
- [ ] **Step 4:** Commit `feat(hw): board power, buttons, battery and SSD2683 driver`.

---

### Task 7: Wake cycle application

**Files:** Create `src/app.h/.cpp`, `src/net.h/.cpp`, `src/settings.h/.cpp`; modify `src/main.cpp`.

**Interfaces:**
```cpp
struct Settings { String backendUrl; String token; };  // NVS namespace "kitchen", keys "url","token"
Settings loadSettings(); void saveSettings(const Settings&);
bool wifiConnect(uint32_t timeoutMs);                   // stored creds (WiFi.begin()), no portal
bool fetchKitchen(const Settings&, int shift, String& body, int& httpStatus); // GET {url}/kitchen?shift=N, Bearer, 8 s timeout
// RTC_DATA_ATTR state: fingerprint, lastSuccessUtc, lastShownNight (bool), lastKitchenJson? (no — too big; keep last View-relevant summary only if needed)
void runWakeCycle();                                     // the spec's wake cycle; ends in board::deepSleep
```
Flow (spec "Behaviour"): battery critical (<5 %) → renderChargeMe, `deepSleep(0, true)`. Wake=ButtonConfirm → maintenance (Task 8). Phase Night and wake=Timer → render night (fetch shift per `planNight`), sleep until 06:00. Night + ButtonDown → live day screen, then `deepSleep(600)` with an RTC flag so the next timer wake redraws night. Day → fetch shift 0; on success set time from `server_time` (`settimeofday`) and render if fingerprint changed or ButtonDown; on failure keep screen, and if `now - lastSuccessUtc > 45 min` and not yet shown, re-render last-known view with stale marker (keep last parsed body in a PSRAM-free RTC slow mem string ≤ 4 KB, else skip); then `planAfterDayRefresh`. Time source when RTC lost (PowerOn): `server_time` from first fetch; if no fetch possible → sleep 15 min.

- [ ] **Step 1:** Implement with logic decisions delegated to core functions (no new branching logic in Arduino code beyond what is listed).
- [ ] **Step 2:** `pio run -e note4c` → SUCCESS; `pio test -e native` still PASS.
- [ ] **Step 3:** Document state machine diagram (mermaid) in `docs/architecture.md`.
- [ ] **Step 4:** Commit `feat(app): wake cycle with fetch, fingerprint skip, night and battery handling`.

---

### Task 8: Maintenance mode, settings portal, OTA, release

**Files:** Create `src/maintenance.h/.cpp`, `src/ota.h/.cpp` (adapted from `eink-desk-panel/src/ota_update.h`), `.github/workflows/release.yml`, `docs/release.md`.

- [ ] **Step 1:** Maintenance (Confirm wake or first boot without WiFi/settings): render maintenance screen once; start WiFiManager config portal (AP `Kuechenpanel-Setup`, custom params "Backend URL" and "Token", prefilled from NVS; when already connected use `startWebPortal()` on the LAN so settings can be changed at `http://<ip>/`); check GitHub latest release (`githendrik/eink-kitchen-panel`, asset `firmware.bin`, compare `tag_name` != FIRMWARE_VERSION); if newer: show "Update vX verfügbar – Taste ↑ installiert" on the screen (one redraw), Up button installs (streams to OTA partition, then reboot). Exit on Confirm press or 10 min idle → normal wake cycle. Mark app valid (`esp_ota_mark_app_valid_cancel_rollback`) after the first successful fetch.
- [ ] **Step 2:** `release.yml`: on tag `v*`: set `FIRMWARE_VERSION` to the tag (like eink-desk-panel workflow), `pio test -e native`, `pio run -e note4c`, upload `.pio/build/note4c/firmware.bin` as `firmware.bin` to the release.
- [ ] **Step 3:** `pio run -e note4c` → SUCCESS. Document maintenance mode, NVS keys, OTA and release procedure in `docs/architecture.md` + `docs/release.md`.
- [ ] **Step 4:** Commit `feat: maintenance mode, settings portal, OTA and release workflow`.

---

### Task 9: On-device bring-up (controller, with user for WiFi)

- [ ] Flash `note4c`; device has no WiFi creds → maintenance/portal AP `Kuechenpanel-Setup` appears. **User** joins it from a phone, enters WiFi, Backend URL `http://home-apis:3000` (or `http://192.168.1.118:3000`), token.
- [ ] Verify: day screen renders with real calendar + fake solar; serial log shows fingerprint skip on the next wake; Down button forces refresh; Confirm enters maintenance and shows IP.
- [ ] Tag `v0.1.0`, confirm release asset; OTA test: bump to `v0.1.1`, install via maintenance mode.

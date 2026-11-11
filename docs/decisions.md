# Decisions

What was decided, by whom, and why. "User" = the owner's explicit choice;
"agent" = a call made during the build that the owner can revisit. Newest
additions at the bottom of each section.

## Product

| Decision | Why | By |
|---|---|---|
| Two things on screen: family calendar (today + tomorrow) and live solar readings | The two glances wanted at the kitchen counter | User |
| First glance: PV production + today's events. Second glance: tomorrow + home battery state | User's description of the morning glance | User |
| Layout A: yellow solar banner on top, today large below, tomorrow below a divider | Chosen from four mockups (A/B/C + warnings) | User |
| Tomorrow's events one per line (not one condensed line); more padding under banner and between days | Readability after seeing the real panel | User |
| All on-screen text German | Household language, event titles are German | User |
| Red only for warnings: panel battery < 20 %, grid import, home battery < 15 %, stale or missing data | Red has to mean "look at this" | User (concept), agent (thresholds) |
| Dropped "imminent event" red highlight from the mockup | Would need time-based wakes/redraws, costs battery | Agent |

## Power and schedule

| Decision | Why | By |
|---|---|---|
| Refresh every 15 min, only 06:00–22:00 | Battery life; nobody looks at night | User (10–15 min) / agent (15) |
| Skip the redraw when nothing visible changed (fingerprint with rounded values: PV/load/grid 100 W, SoC 5 %) | A 4-colour refresh is slow and costly | Agent |
| No partial refresh | The 4-colour SSD2683 mode has none (vendor firmware never uses one) | Hardware |
| 22:00: night screen with "Gute Nacht" banner + sleeping cat, calendar shifted one day forward, no timed wakes until 06:00 | Next viewer is in the morning | User |
| Button at night: live screen, 10 min later back to night screen | Lets you peek without breaking the night mode | Agent |
| Low panel battery: red bar; critically low: "Akku leer – bitte laden" screen, wake on charger | A dead e-ink panel still looks alive | User (concept) |
| Hold side Down 3 s → "Aus" screen + release power latch (true off on battery); short Down → on again | Avoid drain before solar is installed and on holidays | User (concept), agent (gesture/mechanism, approved) |
| 60 s minimum sleep; near 22:00 render night immediately, near 06:00 treat as day | Avoid wake loops from RTC drift | Agent |
| Charging while critical: 15 min timer, no charger wake bit | CHRG_L stays low while charging → wake loop | Agent (review finding) |

## Buttons

| Button | Function | Why |
|---|---|---|
| Front round (GPIO0) | Maintenance mode: status screen, settings portal, update check; press again to exit | User wanted maintenance + OTA on demand; only GPIO0/18 can wake |
| Side Down (GPIO18) | Short: refresh now. Hold 3 s: off. When off: on | Also the hardware power button |
| Side Up (GPIO39) | In maintenance: install available update | Not wake-capable, so only usable while awake |

## Firmware architecture

| Decision | Why | By |
|---|---|---|
| PlatformIO + Arduino (not ESP-IDF), own SSD2683 driver from the vendor sequence | Matches the owner's other panels (eink-desk-panel), reuses OTA/WiFiManager patterns | Agent |
| Logic in `lib/core` (host-tested), thin Arduino layer in `src/` | Testable without hardware; a simulator renders every screen | Agent |
| Own Latin-1 GFX fonts (DejaVu Sans) instead of Adafruit_GFX/U8g2 | Portable to the host build; umlauts needed | Agent |
| One `GET /kitchen` per wake; the backend aggregates | One short HTTP call per wake is cheapest on battery | User + agent |
| OTA from GitHub releases (`firmware.bin`), only from maintenance mode | Same as the desk panels; never on timed wakes | User |
| Rollback: `verifyRollbackLater()` → mark valid after WiFi connects | Arduino otherwise marks every image valid before `setup()` | Agent (review finding) |
| Toolchain pinned to espressif32 6.12.0 / Arduino 2.0.17 | CI picked up Arduino 3.x and failed to build | Agent |
| Factory flash backed up before first flash (`backup/`, not in git) | Vendor recommendation | Agent |
| Settings (backend URL, token) via WiFiManager portal, stored in NVS | No secrets in a public repo | Agent |

## Rejected / deferred

- **Talking to iCloud/GoodWe from the panel:** rejected — integration churn belongs in the backend.
- **Partial display refresh:** not possible in 4-colour mode.
- **SEMS cloud from the panel:** rejected; solar source is a backend concern (see home-apis decisions).

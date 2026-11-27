# AGENTS.md — eink-kitchen-panel

Battery-powered e-paper kitchen panel (ZECTRIX NOTE4C devkit: ESP32-S3,
4.2" 400×300 black/white/yellow/red display). It wakes every 15 min between
06:00 and 22:00 (Europe/Zurich), fetches one JSON document from the home
backend, draws it, and deep-sleeps. Everything on screen is German.

Backend: [`githendrik/home-apis`](https://github.com/githendrik/home-apis)
(`GET /kitchen`). This repo never talks to iCloud or GoodWe directly. The
backend also sends today's production/consumption `history`, which the day
screen draws as a chart (since v0.2.0).

## Read in this order

1. [`docs/status.md`](docs/status.md) — current version, what works, what's open.
2. [`docs/decisions.md`](docs/decisions.md) — why things are the way they are (read before changing behaviour).
3. [`docs/architecture.md`](docs/architecture.md) — detailed reference: pins, display driver, wake state machine, `history` parsing, chart drawing, screens.
4. [`docs/release.md`](docs/release.md) — releasing, flashing, factory restore.

`docs/superpowers/specs|plans/` hold the design and build plans: the original
panel (2026-11-10) and the history chart (2026-11-26). They are historical:
the chart layout was revised after the plan (no divider, edge to edge, totals
in the footer, v0.2.2–v0.2.3). Where they differ from the docs above, the
docs above win.

## Repository map

| Path | What |
|---|---|
| `lib/core/` | All logic that doesn't touch hardware, plain C++17, no Arduino headers: JSON parsing incl. optional `history` (`kitchen`), schedule (`schedule`, `wake`), view model + German labels + chart view + fingerprint (`view`), chart column heights (`chart`), canvas/fonts/layouts incl. the chart (`canvas`, `text`, `render`, `fonts/`). Host-tested. |
| `src/` | Arduino layer: `main.cpp` (setup), `app.cpp` (wake cycle), `board.cpp` (power latch, buttons, battery, sleep), `epd_ssd2683.cpp` (display driver), `net.cpp` (WiFi/HTTP), `settings.cpp` (NVS), `maintenance.cpp` (status screen, setup portal), `ota.cpp` (GitHub-release updates). |
| `test/` | Unity tests for `lib/core` (`pio test -e native`). |
| `tools/sim/` | Host simulator: renders scenario JSON to `docs/screens/*.png`. `"syntheticHistory": {"until": "HH:MM"}` in a scenario generates a chart day (`day_chart_*.json`). |
| `tools/gen_fonts.py`, `tools/fonts/` | Generates the Latin-1 bitmap fonts in `lib/core/src/fonts/` (`ADVANCE_FIX` hand-tunes single glyph advances). |
| `docs/screens/` | Rendered screens — look at these before touching layouts. |
| `backup/` | Factory flash backup (gitignored, only on the original Mac). |

## Commands

```sh
pio test -e native                 # host unit tests (must pass)
pio run -e note4c                  # firmware build (must pass)
pio run -e note4c-diag             # diagnostic build (colour bars, serial battery log)
pio run -e sim && .pio/build/sim/program tools/sim/scenarios docs/screens   # re-render screens
pio run -e note4c -t upload        # flash via USB (device must be awake or in download mode)
git tag vX.Y.Z && git push origin vX.Y.Z   # release → OTA (see docs/release.md)
```

## Rules

- Run `pio test -e native` and `pio run -e note4c` before every commit.
- Decision logic goes in `lib/core` with a native test; `src/` only executes decisions.
- Layout changes: re-render the simulator and look at the PNGs. Image viewers may cache — compare files or render to a new path.
- Update `docs/architecture.md` in every change that alters behaviour; add a line to `docs/decisions.md` for every non-obvious choice; keep `docs/status.md` current.
- Repo is public: never commit WiFi passwords, tokens or config files with secrets.
- Toolchain is pinned (`espressif32@6.12.0` = Arduino-ESP32 2.0.17, ArduinoJson 7.4.3, WiFiManager commit). Newer platforms (Arduino 3.x) don't build this code — don't unpin casually.
- Create/modify files with editor tools, not shell redirection.

## Gotchas (each cost real debugging time)

- **GPIO17 is a power latch.** Must be HIGH + held from the first line of `setup()` and through deep sleep (`gpio_deep_sleep_hold_en`), or the board switches off on battery. Driving it LOW is how "Aus" works.
- **Only GPIO0 (front) and GPIO18 (side Down) can wake from deep sleep.** GPIO39 (side Up) is not an RTC pin; it is only polled while awake.
- **`WiFi.SSID()` only reports the currently connected network**, not stored credentials — use `esp_wifi_get_config` (see `hasWifiCreds`).
- **Arduino marks OTA images valid before `setup()`** unless `verifyRollbackLater()` returns true (it does, in `main.cpp`); the app marks itself valid after WiFi connects.
- **Serial output at boot is usually lost** (USB CDC re-enumerates after deep sleep). To read logs, keep a listener polling `/dev/cu.usbmodem*` and wake the panel with a button.
- **A 4-colour refresh takes 10–20 s.** Never use the display for progress feedback; the green LED (GPIO3, active low) is used during OTA.
- **The Mac's corporate proxy** (`127.0.0.1:3128`) returns 502 for LAN addresses: use `curl --noproxy '*'` when testing the backend.
- **The last `/kitchen` body is kept in 4000 bytes of RTC memory** (`kBodyMax`, `src/app.cpp`) for offline re-renders. A larger body still renders, but the RTC copy is skipped (logged). The backend keeps `/kitchen` compact and tests a busy day at about 3.4 KB, so check that budget before adding fields.
- **No-chart layout is pinned by a pixel hash** (`test_render_without_chart_unchanged`). If you change the classic day screen on purpose, re-record the hash and say so in the commit.
- **Fonts can't simply be regenerated on this Mac** (no Pillow installed; a different Pillow version may shift glyphs). For single-glyph tweaks, edit `ADVANCE_FIX` in `tools/gen_fonts.py` and the matching line in the generated header together, as was done for the Small `W` ("kWh" gap).

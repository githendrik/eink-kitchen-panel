# Architecture

Firmware for the ZECTRIX NOTE4C (ESP32-S3 N16R8, 400×300 4-colour e-paper) kitchen panel.

## Layout

- `src/main.cpp` – Arduino entry point and hardware glue (currently a stub: board init, log wake/battery, deep sleep 120 s; test pattern only in `note4c-diag`).
- `src/board.*`, `src/epd_ssd2683.*` – power, buttons, battery, display driver.
- `lib/core` – plain C++17, no Arduino headers; builds for `native` and `note4c`.
- `test/` – Unity tests, run with `pio test -e native`.

## Pins

| Function | GPIO |
|---|---|
| Power latch (HIGH + `gpio_hold_en`) | 17 |
| EPD power | 6 |
| EPD SCK / MOSI | 12 / 13 |
| EPD CS / DC / RST | 11 / 10 / 9 |
| EPD BUSY (low = busy) | 8 |
| Button Confirm (active low, RTC) | 0 |
| Button Down (active low, RTC) | 18 |
| Button Up (active low, not RTC) | 39 |
| CHRG_L (low = charging) | 2 |
| STDBY_H | 1 |
| Battery ADC (ADC1_CH3, 12 dB, ×2 divider) | 4 |
| LED (active low) | 3 |
| Audio power / amp (kept off) | 42 / 46 |

## Display

400×300, 2 bits per pixel, MSB first, 100 bytes/row, 30 000-byte buffer.

| Colour | Code |
|---|---|
| BLACK | 0 |
| WHITE | 1 |
| YELLOW | 2 |
| RED | 3 |

## Board and power (`src/board.cpp`)

- `board::begin()` drives GPIO17 HIGH and `gpio_hold_en`s it, keeps EPD power (6), LED (3, high = off), audio power (42) and amp (46) off, and enables pull-ups on buttons and CHRG_L.
- `deepSleep(seconds, wakeOnCharger)`: EPD power low and held, latch held through deep sleep with `gpio_deep_sleep_hold_en()` (so the board stays powered on battery), ext1 `ANY_LOW` wake on GPIO0|GPIO18 (+GPIO2 when `wakeOnCharger`), RTC pull-ups on those pins, optional timer wake (`seconds <= 0` = none).
- Wake mapping: undefined → PowerOn, timer → Timer, ext1 mask bit 0 → ButtonConfirm, bit 18 → ButtonDown, bit 2 → Charger, else Other. Up (GPIO39) is not an RTC pin: it can only be polled while awake.
- Battery: ADC1_CH3, 12 dB, `esp_adc_cal` (eFuse calibration; Arduino-ESP32 2.x/IDF 4.4 has no curve-fitting scheme), ×2 divider, mean of 10 reads.

## SSD2683 driver (`src/epd_ssd2683.cpp`)

`Epd::show(buf)` runs: GPIO6 high → SPI3 at 10 MHz mode 0 with manual CS → RST high/low (20 ms)/high → wait BUSY → `0xE9 0x01` → `0x10` + 300 rows of 100 bytes → `0x04`, wait BUSY → `0x12 0x00`, wait BUSY → `0x02 0x00`, wait BUSY → `0x07 0xA5` (deep sleep) → SPI released, GPIO6 low and held. BUSY low = busy, timeout 120 s (logged, then continues).

`note4c-diag` (`-DDIAG_TEST_PATTERN`) draws four bars (black/white/red/yellow) plus "Ä Ö Ü ß Test", logs wake cause, battery, charging, and sleeps 120 s.

### Hardware verification

TODO (controller): record bar colours, umlaut legibility, battery mV, wake cause logging, Down-button wake.

## Rendering (`lib/core`)

- `Canvas` (`canvas.h`) wraps a caller-owned 30 000-byte buffer; all drawing is clipped.
- `drawText`/`textWidth` (`text.h`) decode UTF-8 to Latin-1 code points; U+2026 "…" maps to code 0x7F, anything else unknown becomes `?`. Text longer than `maxW` is truncated with "…".
- Fonts (`lib/core/src/fonts/`) are generated Adafruit-GFX style `const` headers from DejaVu Sans / Bold (1-bit, no antialiasing): Small 11, Body 14, BodyBold 14 bold, Title 18 bold, Huge 40 bold px. Covers 0x20–0x7E, 0xA0–0xFF and "…".
- Regenerate: `python3 -m venv venv && venv/bin/pip install pillow && venv/bin/python tools/gen_fonts.py`. TTFs and licence are in `tools/fonts/`.

## Data model and schedule (`lib/core`)

- `kitchen.h`: `parseKitchen` turns the `/kitchen` JSON into `Kitchen` (`today`, `tomorrow`, optional `solar`, `serverTimeUtc`). Invalid JSON or missing `calendar.today`/`tomorrow` returns false with `err` set. `solar: null` gives no `Solar`. A solar `updated_at` that fails to parse is treated as stale. `parseIsoUtc` handles `Z`/`±HH:MM` offsets with its own days-from-civil (no `timegm`).
- `battery.h`: `batteryPercent(mV) = (-v² + 9016v − 19189000)/10000`, clamped 0–100.
- `schedule.h`: local time via `setenv("TZ")`/`tzset`/`localtime_r`/`mktime` with a POSIX TZ string (`CET-1CEST,M3.5.0,M10.5.0/3`).
  - Day phase is 06:00–22:00 local (`phaseAt`).
  - `planAfterDayRefresh`: sleep `min(900 s, time to 22:00)`, shift 0; outside the day window it defers to `planNight`.
  - `planNight`: sleep until the next 06:00 local (DST-aware); `shift=1` from 22:00 to midnight, `0` after midnight.

## View model (`lib/core/src/view.h`)

`buildDayView` / `buildNightView` turn `Kitchen` into a render-ready `View` (German UTF-8 text, arrows kept; the renderer maps unsupported glyphs). Powers are shown in kW with one decimal (half away from zero). `headerDate` uses `today.date` ("Di 10. Nov"). Labels are "Heute"/"Morgen"; if `today.date` differs from the local date of `now` (night view, shift=1) they are "Morgen"/"Übermorgen". All-day events are listed first ("ganztägig"), then timed ones ("HH:MM").

| Warning (red) | Condition |
|---|---|
| `warnGridImport` | `grid_w > 50` (≤ 50 is no warning) |
| `warnHomeBatteryLow` | home battery SoC < 15 % |
| `warnPanelBattery` | panel battery < 20 % |
| `solarStale` | backend `stale` or `updated_at` > 30 min before now (shows "Stand HH:MM") |
| `solarMissing` | `solar` is null |
| `dataStale` | `lastSuccessUtc` > 45 min before now; `lastSuccessUtc == 0` ("never") is not stale; never set in the night view |

Battery arrow: `battery_w ≥ 50` "↑", `≤ −50` "↓", otherwise none.

`fingerprint` is FNV-1a over: screen, header date, PV/load/grid rounded to 100 W, grid direction, SoC in 5 % steps (floor), arrow, all warning flags, labels and all event times/titles. It excludes `updatedAt` and clock times, so a refresh only redraws on visible change.

## Screens (`lib/core/src/render.h`)

- `renderDay`: yellow solar banner (92 px, 3 px black bottom border): "Solar", Huge kW number, house/grid/battery (arrows drawn as shapes, since the fonts lack "→←↑↓") and a SoC bar. Below: today header, up to 5 event lines (Body, 19 px pitch), tomorrow condensed on up to 3 Small lines, update time bottom-right. With `warnPanelBattery` a 24 px red bar ("Panel-Akku schwach - bitte laden", ASCII hyphen because the font lacks an en dash) sits on top and the banner shrinks to 68 px. Red text: grid import, home battery < 15 %, stale solar "Stand", stale data footer. Missing solar shows "keine Daten" in the banner.
- Overflow: more than 5 events today → 4 events plus "+N weitere" (`visibleTodayLines` returns 4).
- `renderNight`: calendar (shifted labels) plus a top "Gute Nacht" banner (yellow, same 92 px / 68 px compact geometry as the day banner, red warning bar above when the panel battery is low) with a sleeping cat (`cat_bitmap.h`, drawn with primitives, white body with black outline). `renderChargeMe`: red frame and "Akku leer / bitte laden". `renderMaintenance`: IP, version, WLAN, update, battery, last fetch.
- Simulator: `pio run -e sim && .pio/build/sim/program tools/sim/scenarios docs/screens` renders each scenario JSON (`screen`, `now`, `panelBatteryPct`, `lastSuccess`, `kitchen` body, `maint`) to a PNG with a built-in encoder.

| | |
|---|---|
| ![day_export](screens/day_export.png) | ![day_import_lowbatt](screens/day_import_lowbatt.png) |
| ![day_many_events](screens/day_many_events.png) | ![day_solar_missing](screens/day_solar_missing.png) |
| ![day_stale](screens/day_stale.png) | ![night](screens/night.png) |
| ![charge_me](screens/charge_me.png) | ![maintenance](screens/maintenance.png) |

## Environments

- `sim` – host PNG simulator (`tools/sim`).
- `note4c` – device firmware (OTA-capable partition table, two 3 MB app slots).
- `note4c-diag` – `note4c` plus `-DDIAG_TEST_PATTERN`.
- `native` – host unit tests for `lib/core`.

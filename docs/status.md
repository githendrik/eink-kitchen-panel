# Status

_Last updated: 2026-11-27._

## In one paragraph

The panel is installed and working: firmware **v0.2.3** (installed via OTA),
fed by `home-apis` **v0.3.0** on the homelab. The day screen shows:
- a solar banner with **real GoodWe data** from the SEMS+ cloud (battery
  installed; PV panels still being mounted, so PV reads 0);
- today's family calendar (real iCloud data);
- **today's production/consumption chart** instead of "Morgen". Production is
  the yellow area, consumption the black line, and red marks where the house
  uses more than it makes. The chart runs edge to edge directly below the
  calendar, with totals, scale and update time in the footer.

The night screen is unchanged. The panel can be switched off by holding the
side Down button for 3 s.

## Versions

| Component | Version | Where |
|---|---|---|
| Firmware | v0.2.3 | GitHub release `firmware.bin`; installed on the device via OTA |
| Backend | home-apis v0.3.0 (`solar_source: sems`, `/kitchen` history) | LXC CT 110 `home-apis` on host `lab`, port 3000 |
| Old calendar proxy | icloud-calendar-api (Deno) v0.1.0 | CT 116 `calendar-api`, still serving the desk panels |

## Verified on the device (2026-11-11)

- Kitchen screen with real calendar + fake solar; night screen logic (simulator).
- Front button → maintenance screen; side Down → refresh; hold Down 3 s → "Aus" (switches off on battery) and short Down → back on.
- Setup portal `Kuechenpanel-Setup` (WiFi, backend URL, token).
- OTA via maintenance + Up (v0.1.1 → v0.1.2 → v0.1.3 → v0.1.4); LED blinks during install from v0.1.3 on.
- Battery reading 4140–4164 mV (99–100 %) on USB.

## Verified on the device (2026-11-26/27)

- Real GoodWe data in the banner (home-apis v0.2.0+).
- History chart with real data (v0.2.1 → v0.2.3 via OTA); the owner confirmed the final layout works.

## Device configuration (not in git)

Stored in NVS on the panel, entered via the setup portal: WiFi credentials,
backend URL `http://home-apis:3000` (resolves via the Internet-Box DNS to the
CT 110 address), bearer token (same as the backend's `api_token`).

## Open items

1. **Calendar cut-over (backend repo, ~2026-11-14):** compare log in CT 110 must show only the expected "Putzfrau" time difference; then move the `calendar-api` name to CT 110, stop CT 116. Desk panels then show correct times for recurring events. Steps: Obsidian homelab log `2026-11-10-home-apis-deploy`.
2. **Solar signs on real data (backend):** confirm `battery_w` > 0 while charging once the battery moves, and the PV/export sign once the panels produce. Fixes go in `home-apis` (`sems.go`); the firmware needs no change. Expect the red "home battery < 15 %" warning and an all-red chart until the panels produce (battery sits at 10 %).
3. **Battery thresholds:** warning < 20 %, critical < 5 % are first guesses; observe a real discharge and tune.
4. **One unexplained false "Akku leer"** on the very first boot after USB flashing (later readings were 100 %). Not reproduced; if it recurs, re-measure before showing the charge screen.
5. **Minor polish:** en dash missing in the font (ASCII hyphen used); `setInsecure()` for GitHub/OTA TLS; no feedback on screen when a Down-press refresh fails.
6. **History chart, to watch:** how the red areas read once there's real production, and whether the redraw on most daytime wakes costs noticeable battery (throttle to 30 min if so, see `docs/decisions.md`).

## Recently fixed (for context)

- v0.1.1: setup portal reopened after every restart (`WiFi.SSID()` misuse).
- v0.1.2: tomorrow's events one per line, more vertical padding.
- v0.1.3: LED feedback during OTA.
- v0.1.4: hold Down 3 s to switch off.
- v0.2.0 (2026-11-26): today's production/consumption chart replaces "Morgen" on the day screen (`history` from home-apis v0.3.0). Design: `docs/superpowers/specs/2026-11-26-solar-history-chart-design.md`, plan: `docs/superpowers/plans/2026-11-26-solar-history-chart.md`.
- v0.2.1: 3 px divider above the chart (removed again in v0.2.2).
- v0.2.2: no divider; chart edge to edge directly below the calendar; totals, kW scale and update time moved to the footer.
- v0.2.3: dotted "now" line shortened to the lower 80 % of the chart.

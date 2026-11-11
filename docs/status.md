# Status

_Last updated: 2026-11-11._

## In one paragraph

The panel is installed and working: firmware **v0.1.4**, fed by `home-apis`
**v0.1.1** on the homelab. It shows today's and tomorrow's family calendar
(real iCloud data) and a solar banner with **fake data** — the GoodWe solar
system isn't installed yet. Until it is, the panel can be switched off by
holding the side Down button for 3 s.

## Versions

| Component | Version | Where |
|---|---|---|
| Firmware | v0.1.4 | GitHub release `firmware.bin`; installed on the device via OTA |
| Backend | home-apis v0.1.1 | LXC CT 110 `home-apis` on host `lab`, port 3000 |
| Old calendar proxy | icloud-calendar-api (Deno) v0.1.0 | CT 116 `calendar-api`, still serving the desk panels |

## Verified on the device (2026-11-11)

- Kitchen screen with real calendar + fake solar; night screen logic (simulator).
- Front button → maintenance screen; side Down → refresh; hold Down 3 s → "Aus" (switches off on battery) and short Down → back on.
- Setup portal `Kuechenpanel-Setup` (WiFi, backend URL, token).
- OTA via maintenance + Up (v0.1.1 → v0.1.2 → v0.1.3 → v0.1.4); LED blinks during install from v0.1.3 on.
- Battery reading 4140–4164 mV (99–100 %) on USB.

## Device configuration (not in git)

Stored in NVS on the panel, entered via the setup portal: WiFi credentials,
backend URL `http://home-apis:3000` (resolves via the Internet-Box DNS to the
CT 110 address), bearer token (same as the backend's `api_token`).

## Open items

1. **Calendar cut-over (backend repo, ~2026-11-14):** compare log in CT 110 must show only the expected "Putzfrau" time difference; then move the `calendar-api` name to CT 110, stop CT 116. Desk panels then show correct times for recurring events. Steps: Obsidian homelab log `2026-11-10-home-apis-deploy`.
2. **Real solar source:** after the GoodWe is installed. First decide network placement (see backend `docs/decisions.md`). Firmware needs no change — the `/kitchen` contract stays.
3. **Battery thresholds:** warning < 20 %, critical < 5 % are first guesses; observe a real discharge and tune.
4. **One unexplained false "Akku leer"** on the very first boot after USB flashing (later readings were 100 %). Not reproduced; if it recurs, re-measure before showing the charge screen.
5. **Minor polish:** en dash missing in the font (ASCII hyphen used); `setInsecure()` for GitHub/OTA TLS; no feedback on screen when a Down-press refresh fails.

## Recently fixed (for context)

- v0.1.1: setup portal reopened after every restart (`WiFi.SSID()` misuse).
- v0.1.2: tomorrow's events one per line, more vertical padding.
- v0.1.3: LED feedback during OTA.
- v0.1.4: hold Down 3 s to switch off.

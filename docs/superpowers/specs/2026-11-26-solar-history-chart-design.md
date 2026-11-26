# Solar history chart on the kitchen panel — design

_2026-11-26. Spans `eink-kitchen-panel` (firmware) and `home-apis` (backend)._

## Intent

At a glance at the kitchen counter, see how today's solar production went: when it
started in the morning, when it peaked, where clouds cut in, and — against the
house's consumption — when the house used more than the panels made. It's about
getting a feel for the system, not exact numbers.

## What the owner decided

- **Layout A3** (chosen from mockups and checked with a pixel-exact spike, see
  "Spike" below): the day screen keeps the yellow solar banner and today's
  calendar; the **"Morgen" section is removed** from the day screen and the chart
  takes its place. Tomorrow stays on the night screen (from 22:00) and on the
  other panels in the house.
- **Chart content:** today 06:00–22:00 on a fixed time axis, filling left to
  right as the day goes on.
  - Production: yellow filled area with a 1 px black outline.
  - Consumption: 2 px black line.
  - **Red fill** between the two wherever consumption is higher than
    production. This deliberately extends the "red = look at this" rule (see
    `docs/decisions.md`): red now also marks "the house uses more than it
    makes", which will be visible every morning and evening.
- The SEMS+ cloud is the data source (already decided; the inverter stays on the
  guest WiFi).

## Goals

- Show today's production and consumption curve with ~5 min resolution.
- Show today's totals: produced and consumed kWh.
- Keep the current day screen working unchanged when no history is available.
- No measurable battery-life loss beyond the refreshes the chart itself causes
  (see "Refresh frequency").

## Non-goals

- No history beyond today, no yesterday comparison, no forecast.
- No battery or grid curve (only production and consumption).
- No interaction (no zoom, no paging).
- No change to the night, maintenance, off or charge screens.

## Anti-goals

- The panel must never wait on GoodWe: `/kitchen` still answers from cache.
- A broken or missing history must never break the rest of the day screen.
- No secrets or GoodWe identifiers in the `/kitchen` response.

## Layout (400×300, from the spike)

Below today's events, where the "Morgen" block used to start (separator line
stays, same position formula as today):

| Element | Position |
|---|---|
| Separator line | as now (`drawCalendar`), at y = 210 with 4 events |
| Totals, left: `Erzeugt 40 kWh · Verbraucht 12 kWh` | Small font, baseline sep + 15 |
| Scale, right: `6 kW` (larger of the day's production and consumption peak, rounded up to whole kW, min 1) | Small font, same baseline |
| Plot | x 12–388 (376 px = 16 h). Bottom row fixed at y = 276. Top = totals baseline + 7, so the plot is 44 px tall with 4 event lines and **grows taller with fewer events** (about 120 px with none) |
| Baseline | 1 px black at y = 277 |
| "Now" marker | dotted vertical line at the current time (omitted ≥ 22:00) |
| Hour labels `06 09 12 15 18` with 2 px ticks | Small font, baseline 289 |
| Updated time (existing footer) | unchanged, right, baseline 296 |

- **Today's events are capped at 4 lines when the chart is shown** (5 events or
  more: 3 events + "+N weitere"), so the chart never shrinks. Without a chart,
  today keeps its current maximum of 5 lines and "Morgen" is shown as now.
- **Per-column drawing:** each of the 376 columns covers 2.55 min. Column value
  = the value of the 5-minute bin it falls into, linearly interpolated between
  neighbouring bin centres (so 5-min steps don't look stair-stepped). Heights
  = `round(value / scale * plot height)`.
  - Yellow from the bottom up to production height.
  - Red from production up to consumption height when consumption is higher.
  - Production outline: 1 px black at the top of the yellow, joined
    vertically to the previous column.
  - Consumption: 2 px black, joined vertically to the previous column.
- Columns after "now" stay white. Bins without data (missing in the
  response) are treated as "no data": nothing is drawn for those columns, and
  the outlines restart after the gap.
- Small-font kerning: "kWh" renders with a visible gap between "W" and "h" in
  the spike; fix while building (font table or text tweak).

## `/kitchen` contract change (backend → panel)

New optional top-level field `history`. It's present only for the day request
(`shift=0`) and only when the backend has history for **today** (local date in
the configured timezone). Otherwise it is absent or `null`, and the panel draws
today's layout without the chart, as it does now.

```json
"history": {
  "date": "2026-11-26",
  "start": "06:00",
  "step_min": 5,
  "unit_w": 100,
  "pv":   [0,0,0,1,3,7, ...],
  "load": [4,4,5,16,...],
  "pv_wh": 40460,
  "load_wh": 11800,
  "updated_at": "2026-11-26T19:30:00Z"
}
```

- `pv` / `load`: one integer per 5-minute bin from 06:00 local, in units of
  `unit_w` (100 W). Each value is the bin's mean power, rounded. Up to 192 bins
  (06:00–22:00); arrays end at the last complete bin. Both arrays have the same
  length. A bin without data is `null`.
- `pv_wh` / `load_wh`: today's totals from midnight, from the per-minute data
  (more exact than summing bins).
- `updated_at`: when the backend last fetched the history successfully (UTC).
- **Size budget:** the panel keeps the last response in RTC memory for
  offline re-renders, max **4000 bytes** (`kBodyMax` in `src/app.cpp`). At
  100 W units the arrays are mostly 1–2 digits: ~192 × 2 × 3 ≈ 1.2 KB, plus a
  calendar response of ~1.5–2.5 KB. The backend test must assert a realistic
  full day (5+5 events with long titles, 192 bins of 4-digit-W loads) stays
  under 4000 bytes, and the firmware should log when a body doesn't fit.
  **If a body is still too large** (many long titles), the existing behaviour
  stands: the panel renders it normally and only skips the RTC copy, so an
  offline re-render isn't possible until a smaller body arrives. `kBodyMax`
  isn't raised and the backend doesn't truncate anything. _(Agent's call:
  current v0.1.4 already handles oversized bodies this way, see
  `rememberBody` in `src/app.cpp`.)_

## Backend (`home-apis`)

- New `solar.History` type plus an optional interface implemented by sources
  that can provide it: `History(ctx, date) (History, error)`.
- **SEMS:** `POST {api}/sems-plant/api/v1/hems/power/statisticsAndPreV2` with
  items `["pSystem","pConsum"]`, `timeScale:1`, `timeZone:1`, today 00:00:00 to
  23:59:59 local (shape documented in `home-apis/docs/architecture.md`;
  response: `dataList[{item, unit:"kW", powerData[{tp, power}]}]`, one point per
  minute up to now, plus a trailing `power: null` sentinel). Uses the same login,
  token, signature and backoff rules as the flow call (shared client).
  Binning: per 5-minute bin from 06:00, mean of the non-null minutes (W);
  totals from all minutes since 00:00.
- **Fake source:** deterministic history from the same formulas as the fake
  snapshot, so the simulator and dev setups show a chart.
- **Poller:** history runs on its **own schedule, at most every 5 minutes**,
  whatever `solar_poll_seconds` is (the default is 60 s). With the LXC's 300 s
  setting that's 2 requests per 5 minutes. A history failure is logged and keeps
  the last good history for the same date. It never fails the snapshot read
  itself. **A GoodWe rate limit or rejected login hit by either call pauses both**
  (the limit is per account, so the client keeps one shared wait). At local
  midnight the old day's history is dropped. _(Agent's call, from the builder
  check.)_
- `/health` gets a `history` status (`last_success`, `last_error`), like `solar`.
- `--dump-solar` also prints the history summary (bin count, totals).

## Firmware (`eink-kitchen-panel`)

- `lib/core/kitchen`: parse optional `history` into `std::optional<History>`
  (date, start minute, step, unit, two `std::vector<int16_t>` with a "missing"
  marker, totals). Reject malformed history by dropping it, never by failing
  the whole parse.
- `lib/core/view`: `View` gains chart data, filled only when the history date is
  the panel's local today. It holds the bins in watts, the totals texts, the
  scale in kW, the "now" minute and the hour labels. `fingerprint()` mixes in
  the chart (see "Open questions" for how coarsely).
- `lib/core/render`: `renderDay` draws the chart instead of "Morgen" when chart
  data is present (layout above); today is capped at 4 lines in that case.
- Simulator scenarios: add at least `day_chart_morning` (07:30), `day_chart_full`
  (20:30, with red evening), `day_chart_5_events` (cap at 4 lines) and
  `day_chart_missing` (no history → old layout). Re-render `docs/screens/`.
- Unit tests in `test/` for: history parsing (valid, missing, malformed, wrong
  date), bin → column mapping, red/production/consumption heights, scale
  rounding, 4-line cap, fingerprint behaviour.

## Rollout order

1. Backend first. `history` is an additive field, and the current firmware
   (v0.1.4) ignores it: `parseKitchen` reads only known keys, and a body over
   4000 bytes only skips the offline RTC copy (checked by the builder check).
2. Firmware release via a tag (GitHub release, OTA from maintenance mode), or
   flashed over USB.
3. Docs in both repos (`architecture.md`, `decisions.md`, `status.md`).

## Spike (throwaway, 2026-11-26)

`.superpowers/spike/chart_spike.cpp` (gitignored) rendered the day screen with
this chart through the real `lib/core` canvas, fonts and `renderDay`, using
synthetic data. Result: readable at 400×300 with four colours. The stepped edges
are fine, the 2 px consumption line is clear, and a 1–3 px red sliver in a short
spike is still visible. Its per-column code may guide the real one, but it was
not test-driven and must be rewritten with tests.

## Refresh frequency (agent's call)

The panel skips redraws when the fingerprint is unchanged (a 4-colour refresh
takes 10–20 s). The fingerprint already includes production, load and grid
rounded to 100 W, which change on almost every daytime wake, so the day screen
already redraws nearly every 15 minutes. The chart therefore adds few extra
refreshes. `fingerprint()` mixes in the chart's bin count and the bin values
(100 W units), so a redraw also happens when only the chart grew. There's no
extra throttling. If a real discharge shows a battery problem, throttle then
(e.g. redraw at most every 30 min).

## Left to the builder

- Exact Small-font kerning fix.
- Code structure inside `render.cpp` / `view.cpp`, names, test layout.
- Backend: whether history is cached per date in the poller or in the source.

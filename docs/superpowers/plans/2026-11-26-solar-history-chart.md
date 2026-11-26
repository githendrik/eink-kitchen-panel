# Solar History Chart Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show today's production/consumption curve (layout A3) on the kitchen panel's day screen, fed by a new `history` field in `home-apis` `/kitchen`.

**Architecture:**
- **Backend:** builds 5-minute bins from the SEMS+ per-minute power curve, on its own ≤ 5-min schedule in the solar poller, and adds them to `/kitchen`.
- **Firmware:**
  - parses the optional field;
  - turns it into a `ChartView` (pure, host-tested);
  - computes per-pixel-column heights (pure, host-tested);
  - draws the chart in place of "Morgen".

**Tech Stack:** Go 1.24 (`home-apis`), C++17 + ArduinoJson 7 + Unity (`eink-kitchen-panel`, PlatformIO).

**Spec:** `docs/superpowers/specs/2026-11-26-solar-history-chart-design.md` (in `eink-kitchen-panel`). Read it first; this plan doesn't repeat its rationale.

## Global Constraints

- `/kitchen` answers from cache; history fetching never blocks a request or the snapshot.
- `history` JSON: `date` (local `YYYY-MM-DD`), `start` `"06:00"`, `step_min` `5`, `unit_w` `100`, `pv`/`load` int arrays (bin mean W / 100, rounded; `null` = no data; max 192 bins; only complete bins; equal length), `pv_wh`, `load_wh` (int, since 00:00 local), `updated_at` (UTC RFC3339 seconds).
- `history` only for `shift=0` and only when its `date` is today in `cfg.Timezone`; otherwise the key is absent.
- Realistic full `/kitchen` body ≤ 4000 bytes (firmware `kBodyMax`).
- History fetched at most every 5 min. A rate limit or rejected login from either SEMS call pauses both (shared `waitUntil`).
- Plot area: x 12 to 388 (376 columns = 06:00–22:00). Bottom row fixed at y 276, with the baseline at 277. Top = totals baseline + 7.
- Labels: totals at the left (Small font, baseline sep + 15), scale `N kW` at the right on the same baseline, hours `06 09 12 15 18` (Small font, baseline 289) with 2 px ticks.
- Colours: yellow production fill, red where consumption is higher than production, 1 px black production outline, 2 px black consumption line. Joining and the dotted "now" marker follow the spec.
- Scale = max(production peak, consumption peak) over the day's bins, rounded up to whole kW, at least 1.
- With a chart: today shows at most 4 lines (more than 4 events: 3 + `+N weitere`). Without a chart, the layout stays exactly as it is now.
- On-screen text is German. kWh text is `Erzeugt X kWh · Verbraucht Y kWh`: one decimal below 10 kWh (`3.2`), otherwise whole numbers, with a decimal point (like `2.4 kW`).
- Per-repo rules:
  - `home-apis`: `go vet ./... && go test -race ./... && test -z "$(gofmt -l .)"`
  - `eink-kitchen-panel`: `pio test -e native` and `pio run -e note4c` before every commit, then re-render the simulator and look at the PNGs.
  - Both: update `docs/architecture.md` / `decisions.md` / `status.md`.

## Review Focus

- **Panel wakes at 06:00–06:05** → the history has 0 bins. Expect an axis only, totals `Erzeugt 0.0 kWh · Verbraucht 0.0 kWh`, and no crash or divide-by-zero. Tests: Task 1 `TestBinDayBeforeSix`, Task 8 `columns_empty_history`.
- **History date is yesterday** (a stale RTC body after midnight, or a backend that hasn't refetched) → no chart, old layout. Tests: Task 5 `TestKitchenHistoryOtherDayOmitted`, Task 7 `chart_absent_when_date_not_today`.
- **A gap in GoodWe data** (null minutes, inverter offline) → those columns stay empty and the outlines don't draw a slope across the gap. Tests: Task 1 `TestBinDayNullMinutes`, Task 8 `columns_missing_bins_have_no_data`.
- **Malformed `history`** (string values, unequal lengths, more than 192 bins) → history dropped, rest of the screen fine. Test: Task 6 `parse_history_malformed_is_dropped`.
- **Night request (`shift=1`)** → no `history`. Test: Task 5 `TestKitchenShiftNoHistory`.

---

## Part 1 — `home-apis` (repo `/Users/taarihe1/Repos/github/githendrik/home-apis`, branch `feat/solar-history`)

### Task 1: `solar.History`, JSON encoding, binning

**Files:**
- Create: `internal/solar/history.go`
- Test: `internal/solar/history_test.go`

**Interfaces:**
- Produces:
  - `type Bin struct { W int; OK bool }`
  - `type History struct { Date string; PV, Load []Bin; PVWh, LoadWh int; UpdatedAt time.Time }` with `MarshalJSON` producing the Global Constraints shape (key order `date, start, step_min, unit_w, pv, load, pv_wh, load_wh, updated_at`).
  - `type MinutePoint struct { At time.Time; W float64 }` (missing minutes are simply absent).
  - `func BuildHistory(points []MinutePoint, day time.Time, loc *time.Location, now time.Time) (bins []Bin, wh int)`. `day` is any time on the local day. It returns bins from 06:00 local for each complete 5-min bin with end ≤ `now`, up to 192. Each bin is the mean of its present minutes, rounded to W; `OK=false` if none. `wh` = round(Σ W/60) over the points from 00:00 local to `now`.
  - `const HistoryEvery = 5 * time.Minute`

- [ ] **Step 1: Write failing tests**
  - `TestHistoryJSON`: `History{Date:"2026-11-26", PV:[{1234,true},{0,false}], Load:[{850,true},{0,false}], PVWh:40460, LoadWh:11800, UpdatedAt: 2026-11-26T19:30:00Z}` marshals to exactly `{"date":"2026-11-26","start":"06:00","step_min":5,"unit_w":100,"pv":[12,null],"load":[9,null],"pv_wh":40460,"load_wh":11800,"updated_at":"2026-11-26T19:30:00Z"}`. Note 850 W → 9: round half away from zero.
  - `TestBinDayMeansCompleteBinsOnly`: minutes 06:00–06:12 at 1000 W, now 06:12 → 2 bins, both `{1000,true}`.
  - `TestBinDayBeforeSix`: points 00:00–05:59 at 600 W, now 06:03 → 0 bins, `wh` = 3600.
  - `TestBinDayNullMinutes`: bin 06:05–06:10 with no points → `{0,false}`; neighbours OK.
  - `TestBinDayCapsAt192`: now 23:30 with points all day → `len(bins)==192`.
  - `TestBinDayDSTDay`: on 2026-03-29 (Zurich spring forward), bins still start at 06:00 local.
- [ ] **Step 2:** `go test ./internal/solar/ -run 'History|BinDay'` → FAIL (undefined).
- [ ] **Step 3:** Implement in `history.go`.
- [ ] **Step 4:** Run the same command → PASS.
- [ ] **Step 5:** Commit `feat(solar): History type and 5-minute binning`.

### Task 2: Fake source history

**Files:**
- Modify: `internal/solar/fake.go`
- Test: `internal/solar/solar_test.go`

**Interfaces:**
- Consumes: `BuildHistory`, `MinutePoint`, `History` (Task 1).
- Produces:
  - `type HistorySource interface { History(ctx context.Context) (History, error) }`, defined in `history.go`. The source reads its own clock and uses its local day.
  - The fake source implements it: per-minute points from the same PV and load formulas as `Read`, from 00:00 to now; `UpdatedAt` = now.

- [ ] **Step 1:** Failing test `TestFakeHistory`: at 2026-06-21 13:00 Zurich, `len(PV)==84`, peak PV bin ≥ 6000 W, `PVWh > 0`, `Date=="2026-06-21"`; deterministic across two calls.
- [ ] **Step 2:** Run → FAIL.
- [ ] **Step 3:** Implement. Refactor `Read` so the PV and load formulas are shared helpers `fakePV(h float64, yday int) float64` and `fakeLoad(h float64) float64`. Existing fake tests must stay green.
- [ ] **Step 4:** `go test ./internal/solar/` → PASS.
- [ ] **Step 5:** Commit `feat(solar): fake history`.

### Task 3: SEMS history

**Files:**
- Modify: `internal/solar/sems.go`
- Test: `internal/solar/sems_test.go`

**Interfaces:**
- Consumes: Task 1 types, `HistorySource`.
- Produces: `(*sems).History(ctx) (History, error)`.
  - Request: `POST {api}/sems-plant/api/v1/hems/power/statisticsAndPreV2`, body `{"stationId":…, "items":["pSystem","pConsum"], "timeScale":1, "timeZone":1, "startTime":"YYYY-MM-DD 00:00:00", "endTime":"YYYY-MM-DD 23:59:59", "needDispatchLimit":true}` (local date).
  - Response: `dataList[]{item, powerData[]{tp:"YYYY-MM-DD HH:MM:SS" local, power: kW|null}}`. `pSystem` → PV, `pConsum` → load. Skip `power: null`.
  - Then `BuildHistory` for each series.
  - Same mutex, login/station discovery, re-login-once on expiry, and shared `waitUntil` as `Read`. Generalise `readFlow` into `withSession(ctx, func() error) error`, used by both calls.

- [ ] **Step 1: Write failing tests.** Extend `fakeSems` with `POST /gw/sems-plant/api/v1/hems/power/statisticsAndPreV2`, serving 06:00–06:14 at pSystem 1.2 / pConsum 0.5 plus a trailing `{"tp":"… 23:59:00","power":null,"isCE":true}`.
  - `TestSemsHistoryBinsBothSeries`: at a clock of 06:15 Zurich → PV `[{1200,true},{1200,true},{1200,true}]`, Load `[{500,true}×3]`; request body has the local date and both items; signature valid.
  - `TestSemsHistoryRateLimitPausesFlow`: statistics answers `GY0429` → History errors; a following `Read` within 5 min fails fast without a flow call.
  - `TestSemsHistoryReloginOnExpiry`: the statistics call answers `C0602` once → 2 logins, success.
- [ ] **Step 2:** `go test ./internal/solar/ -run SemsHistory` → FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** `go test -race ./internal/solar/` → PASS (all old SEMS tests too).
- [ ] **Step 5:** Commit `feat(solar): SEMS+ day power curve as history`.

### Task 4: Poller history schedule

**Files:**
- Modify: `internal/solar/poller.go`
- Test: `internal/solar/solar_test.go`

**Interfaces:**
- Consumes: `HistorySource`, `HistoryEvery`.
- Produces:
  - `(*Poller).LatestHistory() (History, bool)`
  - `(*Poller).HistoryStatus() (lastSuccess time.Time, lastErr string)`

  In `pollOnce`, after the snapshot read: if the source implements `HistorySource` and `now - lastHistoryAttempt >= HistoryEvery`, fetch it. Success replaces the stored value; failure keeps it and records the error.

- [ ] **Step 1: Write failing tests** (stub source implementing both interfaces, injected clock).
  - `TestPollerHistoryEveryFiveMinutes`: poll every 60 s for 11 min → 3 history calls (t = 0, 5, 10).
  - `TestPollerHistoryFailureKeepsLast`
  - `TestPollerWithoutHistorySource`: `LatestHistory` ok=false, no panic.
- [ ] **Step 2:** Run → FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** `go test -race ./internal/solar/` → PASS.
- [ ] **Step 5:** Commit `feat(solar): poll history at most every 5 minutes`.

### Task 5: `/kitchen` + `/health` + dump + docs + release

**Files:**
- Modify: `internal/server/server.go`, `internal/server/server_test.go`, `cmd/home-apis/main.go` (dump prints the history summary), `docs/architecture.md`, `docs/decisions.md`, `docs/status.md`, `README.md` (`/kitchen` row).

**Interfaces:**
- Consumes:
  - `SolarProvider` gains `LatestHistory() (solar.History, bool)` and `HistoryStatus() (time.Time, string)`.
  - `kitchenJSON` gains `History *solar.History \`json:"history,omitempty"\``, placed after `solar`.
  - `healthJSON` gains `History statusJSON \`json:"history"\``.

- [ ] **Step 1: Write failing tests** (extend `fakeSolar`).
  - `TestKitchenIncludesTodaysHistory`: now 2026-11-10 12:00 Zurich, history date `"2026-11-10"` → body contains `"history": {` and `"step_min": 5`.
  - `TestKitchenHistoryOtherDayOmitted`: date `"2026-11-09"` → no `"history"` key.
  - `TestKitchenShiftNoHistory`: `?shift=1` → no `"history"` key.
  - `TestKitchenRealisticBodyFitsPanel`: 5 + 5 events with 60-character titles, 192 bins, PV up to 9.9 kW and load up to 12.0 kW, about 10 % nulls → `len(body) <= 4000`.
  - `TestHealthHistoryStatus`.
- [ ] **Step 2:** `go test ./internal/server/` → FAIL.
- [ ] **Step 3:** Implement. Today's date check: `h.Date == s.now().In(s.cfg.Timezone).Format("2006-01-02")`. `--dump-solar` prints `history: <date> bins=<n> pv_wh=<n> load_wh=<n>` when the source implements `HistorySource`.
- [ ] **Step 4:** Full check `go vet ./... && go test -race ./... && test -z "$(gofmt -l .)"` → green.
- [ ] **Step 5: Live check from the Mac** (Keychain env as in v0.2.0, off-VPN preferred):
  - Run `go run ./cmd/home-apis --dump-solar`.
  - Expected: snapshot plus `history: 2026-…` with bins > 0 after 06:05.
- [ ] **Step 6: Docs**
  - `architecture.md`: the `/kitchen` contract with `history`, the SEMS history call (it's no longer "not used yet"), and the poller schedule.
  - `decisions.md`: the 5-min bins in 100 W units (4000-byte panel budget), and the shared rate-limit pause.
  - `status.md`.
- [ ] **Step 7:** Commit `feat: /kitchen history for the panel chart`, merge to `main`, tag `v0.3.0`, push; check that the Release workflow is green.
- [ ] **Step 8: Deploy** (needs `ssh lab`, so not on VPN). Follow the homelab skill:
  - vzdump CT 110;
  - run the installer;
  - `--dump-solar`;
  - `systemctl restart`;
  - check `/health` and that `/kitchen` contains `history`;
  - write the Obsidian daily log.

  If `lab` isn't reachable, stop here and tell the user.

## Part 2 — `eink-kitchen-panel` (repo `/Users/taarihe1/Repos/github/githendrik/eink-kitchen-panel`, branch `feat/history-chart`)

### Task 6: Parse `history`

**Files:**
- Modify: `lib/core/src/kitchen.h`, `lib/core/src/kitchen.cpp`
- Test: `test/test_data/test_main.cpp` (where the `parseKitchen` tests live)

**Interfaces:**
- Produces:
  - `struct History { std::string date; int startMin = 360, stepMin = 5, unitW = 100; std::vector<int16_t> pv, load; int pvWh = 0, loadWh = 0; };`
  - `pv`/`load` hold the raw units from the JSON, with `kNoData = -1` for null.
  - `Kitchen` gains `std::optional<History> history;`.

- [ ] **Step 1: Write failing tests**
  - `parse_history_valid`: date, `startMin==360`, `stepMin==5`, `unitW==100`, `pv=={12,-1}`, `pvWh==40460`.
  - `parse_history_absent`: `!k.history`, parse ok.
  - `parse_history_malformed_is_dropped`, one case each, all with parse ok and `!k.history`:
    - `pv` contains a string;
    - lengths differ;
    - 193 bins;
    - `step_min` 0;
    - `start` `"6"`;
    - a negative value.
- [ ] **Step 2:** `pio test -e native -f test_data` → FAIL.
- [ ] **Step 3:** Implement `readHistory(JsonObjectConst, std::optional<History>&)` in `kitchen.cpp`.
- [ ] **Step 4:** Run → PASS.
- [ ] **Step 5:** Commit `feat(core): parse optional /kitchen history`.

### Task 7: `ChartView` in the view model

**Files:**
- Modify: `lib/core/src/view.h`, `lib/core/src/view.cpp`
- Test: `test/test_view/`

**Interfaces:**
- Consumes: `History` (Task 6).
- Produces:
  - `struct ChartView { bool present = false; int startMin = 360, stepMin = 5; std::vector<int> pvW, loadW; int nowMin = 0; int scaleKw = 1; std::string totalsText, scaleText; };`
  - `pvW`/`loadW` are bin values × `unitW`, with `-1` for no data. `nowMin` = minutes since local midnight.
  - `View` gains `ChartView chart;`.
  - Filled only by `buildDayView`, when `k.history->date` equals the local date of `nowUtc`.
  - `fingerprint()` mixes `chart.present`, the bin count and each bin's raw unit value.

- [ ] **Step 1: Write failing tests**
  - `chart_present_for_today`: `totalsText == "Erzeugt 40 kWh · Verbraucht 12 kWh"` (40460 Wh, 11800 Wh), `scaleText == "6 kW"` (max bin 5500 W), `nowMin == 20*60+30`.
  - `chart_totals_one_decimal_below_10`: 3240 Wh → `"Erzeugt 3.2 kWh · …"`.
  - `chart_scale_uses_load_peak`: PV max 2100 W, load max 3400 W → `"4 kW"`.
  - `chart_scale_min_1`: all zeros → `"1 kW"`.
  - `chart_absent_when_date_not_today`.
  - `chart_absent_on_night_view`.
  - `fingerprint_changes_when_chart_grows`.
- [ ] **Step 2:** `pio test -e native -f test_view` → FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run → PASS.
- [ ] **Step 5:** Commit `feat(core): chart view model`.

### Task 8: Column heights

**Files:**
- Create: `lib/core/src/chart.h`, `lib/core/src/chart.cpp`
- Test: `test/test_render/` (new test cases)

**Interfaces:**
- Consumes: `ChartView`.
- Produces:
  - `struct ChartColumn { bool hasData = false; int pvH = 0, loadH = 0; };`
  - `std::vector<ChartColumn> chartColumns(const ChartView& c, int plotW, int plotH);`
  - Columns map 06:00–22:00 onto `plotW`. Column centre time `t = 360 + (x + 0.5) * 960 / plotW` minutes. Its value is the linear interpolation between neighbouring bin centres (`startMin + i*stepMin + stepMin/2`), using the nearest bin at the edges.
  - If an interpolation partner is missing, use the present one; if both are missing, the column has no data.
  - Columns with `t > nowMin` or beyond the last bin's end have no data.
  - Height = `round(W / (scaleKw*1000) * plotH)`, clamped to `plotH`.

- [ ] **Step 1: Write failing tests**
  - `columns_flat_value`: all bins 3000 W, scale 6, plotH 44 → each column up to "now" has `pvH==22`.
  - `columns_after_now_empty`.
  - `columns_empty_history`: 0 bins → 376 columns, none with data.
  - `columns_missing_bins_have_no_data`: a run of 3 null bins → the columns at their centres have no data.
  - `columns_interpolate`: bins 0 W and 6000 W, scale 6, plotH 60 → the column midway between the two bin centres has `pvH==30 ±1`.
  - `columns_clamp_to_plot`: a value above the scale → `pvH==plotH`.
- [ ] **Step 2:** `pio test -e native -f test_render` → FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run → PASS.
- [ ] **Step 5:** Commit `feat(core): chart column heights`.

### Task 9: Draw the chart, cap today, simulator, docs

**Files:**
- Modify: `lib/core/src/render.cpp`, `lib/core/src/render.h` (`visibleTodayLines` respects the chart cap), `tools/sim/sim.cpp`
- Create: `tools/sim/scenarios/day_chart_morning.json` (07:30), `day_chart_full.json` (20:30), `day_chart_5_events.json`, `day_chart_missing.json` (no history → the old layout with "Morgen")
- Modify: `docs/architecture.md`, `docs/decisions.md`, `docs/status.md`, `docs/screens/*`
- Test: `test/test_render/`

**Interfaces:**
- Consumes: `chartColumns`, `ChartView`.
- Produces: a static `drawChart(Canvas&, const View&, int sep)` in `render.cpp`, drawing per the Global Constraints layout. Each column is drawn bottom-up: yellow `pvH`; then red from `pvH` to `loadH` if `loadH > pvH`; then the 1 px production outline joined to the previous data column; then the 2 px consumption line joined the same way. Joins restart after a column without data.
- Simulator: a top-level scenario key `"syntheticHistory": {"until": "HH:MM"}`. The sim builds a `history` JSON from the spike's synthetic PV and load formulas (`.superpowers/spike/chart_spike.cpp`) for the scenario's date, injects it into `kitchen` before `parseKitchen`, and so exercises the parser too.

- [ ] **Step 1: Write failing tests**
  - `render_chart_replaces_tomorrow`: with a chart, no "Morgen" heading pixels appear (assert the heading's row band below sep contains the chart's yellow, and the old tomorrow baseline y has no text). An easier assertion: pixel `(12, 277)` is black (baseline), and some pixel in the plot is yellow.
  - `render_red_where_load_exceeds_pv`: one bin with load > PV → a red pixel exists in that column.
  - `render_today_capped_at_4_with_chart`: 6 events → `visibleTodayLines(v) == 3`, and "+3 weitere" is drawn.
  - `render_without_chart_unchanged`: a view without a chart renders byte-identically to the current `renderDay` output for the `day_export` scenario. Snapshot the canvas CRC before the change.
- [ ] **Step 2:** `pio test -e native -f test_render` → FAIL.
- [ ] **Step 3:** Implement `drawChart` and the cap. Fix the "kWh" Small-font kerning, which in the spike shows a gap between W and h: check the `h` glyph's xOffset/advance in `tools/gen_fonts.py` output, or render "kWh" with a −1 px tweak in `drawText`, whichever is local. Then add the sim `syntheticHistory` support and the 4 scenarios.
- [ ] **Step 4:** `pio test -e native` and `pio run -e note4c` → green. Run `pio run -e sim && .pio/build/sim/program tools/sim/scenarios docs/screens`, then read the 4 new PNGs plus `day_export.png` (unchanged) and confirm they match the spec layout.
- [ ] **Step 5: Docs**
  - `architecture.md`: the day screen layout, `history` parsing, the chart drawing, the 4-line cap.
  - `decisions.md`: red now also means "consumption > production" (User); "Morgen" removed from the day screen (User); the plot grows with fewer events, the scale is the larger peak, and the 4-line cap (Agent).
  - `status.md`.
- [ ] **Step 6:** Commit `feat: solar history chart on the day screen`.
- [ ] **Step 7: Release and install**
  - Merge to `main`, tag `v0.2.0` (firmware), push, check the release workflow.
  - Install either over USB with `pio run -e note4c -t upload` (the device must be awake: press a button) or via OTA (maintenance button + Up).
  - With the backend deployed, the chart appears on the next wake. Without it, the panel shows the old layout and nothing breaks.

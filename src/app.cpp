#include "app.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <string>

#include "battery.h"
#include "board.h"
#include "canvas.h"
#include "epd_ssd2683.h"
#include "kitchen.h"
#include "maintenance.h"
#include "net.h"
#include "render.h"
#include "settings.h"
#include "view.h"
#include "wake.h"

namespace {
const char* const kTz = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr int64_t kPlausibleUtc = 1700000000;
constexpr int64_t kUnknownTimeSleep = 15 * 60;
constexpr size_t kBodyMax = 4000;

RTC_DATA_ATTR uint32_t rtcFingerprint = 0;
RTC_DATA_ATTR int64_t rtcLastSuccessUtc = 0;
RTC_DATA_ATTR bool rtcStaleShown = false;
RTC_DATA_ATTR bool rtcChargeMeShown = false;
RTC_DATA_ATTR uint16_t rtcBodyLen = 0;
RTC_DATA_ATTR char rtcBody[kBodyMax + 96];

void drawScreen(const View* v, Screen s) {
  uint8_t* buf = (uint8_t*)heap_caps_malloc(Canvas::kBytes, MALLOC_CAP_8BIT);
  if (!buf) {
    Serial.println("framebuffer alloc failed");
    return;
  }
  Canvas c(buf);
  if (s == Screen::ChargeMe) renderChargeMe(c);
  else if (s == Screen::Night) renderNight(c, *v);
  else renderDay(c, *v);
  Epd().show(buf);
  free(buf);
}

void showView(const View& v) {
  drawScreen(&v, v.screen);
  rtcFingerprint = fingerprint(v);
}

void setSystemTime(int64_t utc) {
  timeval tv = {(time_t)utc, 0};
  settimeofday(&tv, nullptr);
}

bool fetchParsed(const Settings& st, int shift, Kitchen& k, String& body) {
  int status = 0;
  if (!fetchKitchen(st, shift, body, status)) {
    Serial.printf("fetch failed, status %d\n", status);
    return false;
  }
  std::string err;
  if (!parseKitchen(body.c_str(), body.length(), k, err)) {
    Serial.printf("parse failed: %s\n", err.c_str());
    return false;
  }
  return true;
}

void rememberBody(const String& body, int64_t now) {
  rtcLastSuccessUtc = now;
  rtcStaleShown = false;
  if (body.length() <= kBodyMax) {
    memcpy(rtcBody, body.c_str(), body.length() + 1);
    rtcBodyLen = body.length();
  } else {
    rtcBodyLen = 0;
  }
}

void rerenderStale(int64_t now, int pct) {
  Kitchen k;
  std::string err;
  if (!parseKitchen(rtcBody, rtcBodyLen, k, err)) return;
  showView(buildDayView(k, now, kTz, pct, rtcLastSuccessUtc));
  rtcStaleShown = true;
}
}  // namespace

void runWakeCycle() {
  setenv("TZ", kTz, 1);
  tzset();

  board::Wake wake = board::wakeCause();
  int pct = batteryPercent(board::batteryMillivolts());
  Settings st = loadSettings();
  int64_t now = time(nullptr);

  WakeInput in;
  in.batteryPct = pct;
  in.confirm = wake == board::Wake::ButtonConfirm;
  in.down = wake == board::Wake::ButtonDown;
  in.settingsComplete = settingsComplete(st);
  // Skip starting the radio for the credential check when the battery-critical path wins anyway.
  if (in.settingsComplete && pct >= kBatteryCriticalPct) in.settingsComplete = hasWifiCreds();
  in.timeKnown = now > kPlausibleUtc;
  if (in.timeKnown) in.phase = phaseAt(now, kTz);

  WakeAction action = decideWake(in);
  if (action == WakeAction::ChargeMe) {
    ChargeMePlan cp = planChargeMe(board::charging(), wake == board::Wake::Charger, rtcChargeMeShown);
    if (cp.draw) {
      drawScreen(nullptr, Screen::ChargeMe);
      rtcChargeMeShown = true;
    }
    board::deepSleep(cp.sleepSeconds, cp.wakeOnCharger);
  }
  if (rtcChargeMeShown) {
    rtcChargeMeShown = false;
    rtcFingerprint = 0;  // force a redraw over the ChargeMe screen
  }
  if (action == WakeAction::Maintenance) runMaintenance();

  WakePlan plan{false, 0, 0};
  if (in.timeKnown) plan = planWake(now, kTz, in.down);
  Kitchen k;
  String body;
  bool wifiUp = wifiConnect(15000);
  if (wifiUp) esp_ota_mark_app_valid_cancel_rollback();
  bool ok = wifiUp && fetchParsed(st, plan.shift, k, body);

  if (!ok) {
    wifiOff();
    if (!in.timeKnown) board::deepSleep(kUnknownTimeSleep, false);
    if (!plan.renderNight && phaseAt(now, kTz) == Phase::Day && rtcLastSuccessUtc != 0 &&
        now - rtcLastSuccessUtc > kStaleAfterSeconds && !rtcStaleShown && rtcBodyLen > 0) {
      rerenderStale(now, pct);
    }
    board::deepSleep(plan.sleepSeconds, false);
  }

  int fetchedShift = plan.shift;
  if (serverTimePlausible(k.serverTimeUtc)) {
    now = k.serverTimeUtc;
    setSystemTime(now);
  } else if (!in.timeKnown) {
    wifiOff();
    board::deepSleep(kUnknownTimeSleep, false);
  }
  plan = planWake(now, kTz, in.down);

  bool render = true;
  if (plan.shift != fetchedShift) {
    Kitchen k2;
    String body2;
    if (fetchParsed(st, plan.shift, k2, body2)) {
      k = k2;
      body = body2;
    } else {
      render = false;  // never draw a screen from data for the wrong shift
    }
  }
  wifiOff();

  if (render) {
    rememberBody(body, now);
    View v = plan.renderNight ? buildNightView(k, now, kTz, pct) : buildDayView(k, now, kTz, pct, now);
    if (in.down || fingerprint(v) != rtcFingerprint) showView(v);
  }
  board::deepSleep(plan.sleepSeconds, false);
}

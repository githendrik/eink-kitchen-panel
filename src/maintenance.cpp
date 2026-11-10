#include "maintenance.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "battery.h"
#include "board.h"
#include "canvas.h"
#include "epd_ssd2683.h"
#include "net.h"
#include "ota.h"
#include "render.h"
#include "settings.h"

namespace {
constexpr int kPinConfirm = 0;
constexpr int kPinUp = 39;
constexpr uint32_t kIdleMs = 10UL * 60UL * 1000UL;
constexpr uint32_t kDebounceMs = 40;
const char* const kApName = "Kuechenpanel-Setup";

void draw(const MaintInfo& m) {
  uint8_t* buf = (uint8_t*)heap_caps_malloc(Canvas::kBytes, MALLOC_CAP_8BIT);
  if (!buf) {
    Serial.println("framebuffer alloc failed");
    return;
  }
  Canvas c(buf);
  renderMaintenance(c, m);
  Epd().show(buf);
  free(buf);
}

bool pressed(int pin) {
  if (digitalRead(pin) != LOW) return false;
  delay(kDebounceMs);
  return digitalRead(pin) == LOW;
}

bool saved = false;
WiFiManagerParameter* pUrl = nullptr;
WiFiManagerParameter* pToken = nullptr;

void onSave() {
  Settings s;
  s.backendUrl = pUrl->getValue();
  s.token = pToken->getValue();
  s.backendUrl.trim();
  s.token.trim();
  if (s.backendUrl.length() > 0 && s.token.length() > 0) saveSettings(s);
  saved = true;
}

[[noreturn]] void finish(bool configured) {
  WiFi.disconnect(true);
  if (configured) esp_restart();  // wake cause becomes PowerOn; configured, so the normal cycle runs
  board::deepSleep(0, false);     // unconfigured: button-only wake, no periodic refresh
}
}  // namespace

void runMaintenance() {
  board::Wake w = board::wakeCause();
  if (w != board::Wake::ButtonConfirm && w != board::Wake::PowerOn) board::deepSleep(0, false);

  pinMode(kPinConfirm, INPUT_PULLUP);
  pinMode(kPinUp, INPUT_PULLUP);

  Settings st = loadSettings();
  bool configured = settingsComplete(st) && hasWifiCreds();

  MaintInfo m;
  m.version = FIRMWARE_VERSION;
  int mv = board::batteryMillivolts();
  m.batteryMv = mv;
  m.batteryPct = batteryPercent(mv);
  m.ip = "-";
  m.updateText = configured ? "pruefe..." : "-";
  m.ssid = configured ? std::string(WiFi.SSID().c_str()) : std::string(kApName);
  m.lastFetch = "-";
  draw(m);  // redraw 1 of max 2

  WiFiManager wm;
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(600);
  String urlDefault = st.backendUrl.length() ? st.backendUrl : String("http://home-apis:3000");
  WiFiManagerParameter urlParam("url", "Backend URL", urlDefault.c_str(), 160);
  WiFiManagerParameter tokenParam("token", "Token", st.token.c_str(), 160);
  pUrl = &urlParam;
  pToken = &tokenParam;
  wm.addParameter(&urlParam);
  wm.addParameter(&tokenParam);
  wm.setSaveConfigCallback(onSave);
  wm.setSaveParamsCallback(onSave);

  OtaInfo ota;
  bool connected = false;
  if (configured) {
    connected = wifiConnect(15000);
    if (connected) {
      wm.startWebPortal();
      m.ip = WiFi.localIP().toString().c_str();
      ota = otaCheckForUpdate();
    }
  } else {
    wm.startConfigPortal(kApName);
    m.ip = WiFi.softAPIP().toString().c_str();
  }

  if (configured) {
    if (!connected) m.updateText = "kein WLAN";
    else if (ota.available) m.updateText = std::string(ota.version.c_str()) + " verfuegbar - Taste Hoch";
    else m.updateText = ota.checked ? "aktuell" : "Pruefung fehlgeschlagen";
  }
  draw(m);  // redraw 2 of max 2

  // The wake press must be released before Confirm counts as exit.
  uint32_t t0 = millis();
  while (digitalRead(kPinConfirm) == LOW && millis() - t0 < 5000) {
    wm.process();
    delay(10);
  }

  uint32_t lastActivity = millis();
  while (millis() - lastActivity < kIdleMs) {
    wm.process();
    if (saved) {
      delay(1500);  // let the portal answer the browser
      finish(settingsComplete(loadSettings()) && hasWifiCreds());
    }
    if (pressed(kPinConfirm)) finish(configured);
    if (ota.available && pressed(kPinUp)) {
      if (otaApplyUpdate(ota)) {
        delay(200);
        esp_restart();
      }
      ota.available = false;  // failed: keep running, no retry loop, no redraw
    }
    delay(10);
  }
  finish(configured);
}

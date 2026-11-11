#include "maintenance.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
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

// Exposes the time of the last HTTP request served by the portal, for the idle timeout.
class Portal : public WiFiManager {
 public:
  unsigned long lastWebAccess() const { return _webPortalAccessed; }
};

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
  Serial.printf("maintenance: url=%u token=%u configured=%d\n", (unsigned)st.backendUrl.length(),
                (unsigned)st.token.length(), (int)configured);

  MaintInfo m;
  m.version = FIRMWARE_VERSION;
  int mv = board::batteryMillivolts();
  m.batteryMv = mv;
  m.batteryPct = batteryPercent(mv);
  m.ip = "-";
  m.updateText = configured ? "prüfe…" : "-";
  m.ssid = configured ? std::string(WiFi.SSID().c_str()) : std::string(kApName);
  m.lastFetch = "-";
  draw(m);  // redraw 1 of max 2

  Portal wm;
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
    Serial.printf("maintenance: wifiConnect=%d\n", (int)connected);
    if (connected) {
      esp_ota_mark_app_valid_cancel_rollback();
      wm.startWebPortal();
      m.ip = WiFi.localIP().toString().c_str();
      ota = otaCheckForUpdate();
    }
  }
  if (!connected) {
    // Unconfigured, or configured but WiFi unreachable: let the user (re)enter WiFi and settings.
    wm.startConfigPortal(kApName);
    m.ip = WiFi.softAPIP().toString().c_str();
    m.ssid = kApName;
  }

  if (configured) {
    if (!connected) m.updateText = "kein WLAN - Setup-Portal";
    else if (ota.available) m.updateText = "Update " + std::string(ota.version.c_str()) + " verfügbar - Taste Hoch";
    else m.updateText = ota.checked ? "aktuell" : "Prüfung fehlgeschlagen";
  }
  draw(m);  // redraw 2 of max 2

  // The wake press must be released before Confirm counts as exit.
  uint32_t t0 = millis();
  while (digitalRead(kPinConfirm) == LOW && millis() - t0 < 5000) {
    wm.process();
    delay(10);
  }

  uint32_t lastActivity = millis();
  unsigned long lastWeb = wm.lastWebAccess();
  while (millis() - lastActivity < kIdleMs) {
    wm.process();
    if (wm.lastWebAccess() != lastWeb) {
      lastWeb = wm.lastWebAccess();
      lastActivity = millis();
    }
    if (saved) {
      delay(1500);  // let the portal answer the browser
      finish(settingsComplete(loadSettings()) && hasWifiCreds());
    }
    if (pressed(kPinConfirm)) finish(configured);
    if (ota.available && pressed(kPinUp)) {
      lastActivity = millis();
      if (otaApplyUpdate(ota)) {
        delay(200);
        esp_restart();
      }
      ota.available = false;  // failed: no retry loop
      m.updateText = "Update fehlgeschlagen";
      draw(m);  // third and last redraw
    }
    delay(10);
  }
  finish(configured);
}

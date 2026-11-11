#include "ota.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>

#include "board.h"

namespace {
const char* const kReleaseUrl = "https://api.github.com/repos/githendrik/eink-kitchen-panel/releases/latest";
const char* const kAssetName = "firmware.bin";
}  // namespace

OtaInfo otaCheckForUpdate() {
  OtaInfo info;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(client, kReleaseUrl);
  http.addHeader("User-Agent", "eink-kitchen-panel");
  int code = http.GET();
  if (code != 200) {
    Serial.printf("OTA: GitHub API returned %d\n", code);
    http.end();
    return info;
  }
  // Only tag and asset fields are needed; the filter keeps the release body out of RAM.
  JsonDocument filter;
  filter["tag_name"] = true;
  filter["assets"][0]["name"] = true;
  filter["assets"][0]["browser_download_url"] = true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("OTA: release JSON parse failed: %s\n", err.c_str());
    return info;
  }
  info.checked = true;
  String tag = doc["tag_name"] | "";
  Serial.printf("OTA: current=%s remote=%s\n", FIRMWARE_VERSION, tag.c_str());
  if (tag.length() == 0 || tag == FIRMWARE_VERSION) return info;
  for (JsonObject a : doc["assets"].as<JsonArray>()) {
    if (String(a["name"] | "") == kAssetName) {
      info.available = true;
      info.version = tag;
      info.binUrl = String(a["browser_download_url"] | "");
      break;
    }
  }
  if (info.binUrl.length() == 0) info.available = false;
  return info;
}

namespace {
// Blinks the status LED while an update downloads (the e-paper is too slow to
// give feedback); switches it off when the update attempt ends.
struct UpdateLed {
  uint32_t last = 0;
  bool on = true;
  UpdateLed() { board::led(true); }
  ~UpdateLed() { board::led(false); }
  void tick() {
    if (millis() - last < 150) return;
    last = millis();
    on = !on;
    board::led(on);
  }
};
}  // namespace

bool otaApplyUpdate(const OtaInfo& info) {
  if (!info.available) return false;
  UpdateLed blink;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(30000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(client, info.binUrl);
  http.addHeader("User-Agent", "eink-kitchen-panel");
  int code = http.GET();
  if (code != 200) {
    Serial.printf("OTA: download failed, HTTP %d\n", code);
    http.end();
    return false;
  }
  int len = http.getSize();
  if (len <= 0 || !Update.begin(len)) {
    Serial.printf("OTA: cannot start (len %d): %s\n", len, Update.errorString());
    http.end();
    return false;
  }
  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[1024];
  int written = 0;
  uint32_t lastData = millis();
  while (written < len) {
    blink.tick();
    int avail = stream->available();
    if (avail <= 0) {
      if (millis() - lastData > 30000 || !stream->connected()) break;
      delay(1);
      continue;
    }
    int n = stream->readBytes(buf, min((int)sizeof(buf), avail));
    if (n <= 0) break;
    lastData = millis();
    if (Update.write(buf, n) != (size_t)n) {
      Serial.printf("OTA: write failed: %s\n", Update.errorString());
      Update.abort();
      http.end();
      return false;
    }
    written += n;
  }
  http.end();
  if (written != len) {
    Serial.printf("OTA: truncated download %d/%d\n", written, len);
    Update.abort();
    return false;
  }
  if (!Update.end(true)) {
    Serial.printf("OTA: finalize failed: %s\n", Update.errorString());
    return false;
  }
  Serial.println("OTA: update written");
  return true;
}

#include "net.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace {
constexpr int kMaxBody = 16 * 1024;
}
bool hasWifiCreds() {
  WiFi.mode(WIFI_STA);
  return WiFi.SSID().length() > 0;
}

bool wifiConnect(uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start >= timeoutMs) {
      Serial.println("WiFi timeout");
      return false;
    }
    delay(100);
  }
  return true;
}

void wifiOff() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

bool fetchKitchen(const Settings& s, int shift, String& body, int& httpStatus) {
  String url = s.backendUrl;
  while (url.endsWith("/")) url.remove(url.length() - 1);
  url += "/kitchen?shift=" + String(shift);

  HTTPClient http;
  http.setTimeout(8000);
  WiFiClientSecure tls;
  WiFiClient plain;
  bool ok;
  if (url.startsWith("https://")) {
    tls.setInsecure();
    ok = http.begin(tls, url);
  } else {
    ok = http.begin(plain, url);
  }
  if (!ok) {
    httpStatus = -1;
    return false;
  }
  http.addHeader("Authorization", "Bearer " + s.token);
  httpStatus = http.GET();
  if (httpStatus != 200 || http.getSize() > kMaxBody) {
    http.end();
    return false;
  }
  body = http.getString();
  http.end();
  return body.length() > 0 && body.length() <= kMaxBody;
}

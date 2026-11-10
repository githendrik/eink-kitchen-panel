#include "settings.h"

#include <Preferences.h>

Settings loadSettings() {
  Settings s;
  Preferences p;
  if (p.begin("kitchen", true)) {
    s.backendUrl = p.getString("url", "");
    s.token = p.getString("token", "");
    p.end();
  }
  return s;
}

void saveSettings(const Settings& s) {
  Preferences p;
  if (!p.begin("kitchen", false)) return;
  p.putString("url", s.backendUrl);
  p.putString("token", s.token);
  p.end();
}

bool settingsComplete(const Settings& s) { return s.backendUrl.length() > 0 && s.token.length() > 0; }

#pragma once
#include <Arduino.h>

struct Settings {
  String backendUrl;
  String token;
};

Settings loadSettings();
void saveSettings(const Settings& s);
bool settingsComplete(const Settings& s);

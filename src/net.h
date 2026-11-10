#pragma once
#include <Arduino.h>

#include "settings.h"

bool hasWifiCreds();
bool wifiConnect(uint32_t timeoutMs);
void wifiOff();
bool fetchKitchen(const Settings& s, int shift, String& body, int& httpStatus);

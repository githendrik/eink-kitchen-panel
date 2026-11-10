#pragma once
#include <Arduino.h>

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

struct OtaInfo {
  bool checked = false;    // GitHub answered with a parseable release
  bool available = false;  // tag differs from FIRMWARE_VERSION and firmware.bin is attached
  String version;
  String binUrl;
};

// Needs a connected WiFi. TLS is not verified (setInsecure): the release is public and the
// device has no CA store; integrity relies on GitHub's HTTPS endpoint only.
OtaInfo otaCheckForUpdate();

// Streams the asset into the next OTA partition. Returns true when finished (caller restarts).
bool otaApplyUpdate(const OtaInfo& info);

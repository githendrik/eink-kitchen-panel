#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include "core.h"

static const gpio_num_t PIN_POWER_LATCH = GPIO_NUM_17;

void setup() {
  gpio_set_direction(PIN_POWER_LATCH, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_POWER_LATCH, 1);
  gpio_hold_en(PIN_POWER_LATCH);

  Serial.begin(115200);
  delay(500);
  Serial.printf("eink-kitchen-panel %s (core %d)\n", FIRMWARE_VERSION, core_version());

  esp_sleep_enable_timer_wakeup(60ULL * 1000000ULL);
  esp_deep_sleep_start();
}

void loop() {}

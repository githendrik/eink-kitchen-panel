#include "maintenance.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "battery.h"
#include "board.h"
#include "canvas.h"
#include "epd_ssd2683.h"
#include "render.h"

void runMaintenance() {
  board::Wake w = board::wakeCause();
  if (w != board::Wake::ButtonConfirm && w != board::Wake::PowerOn) board::deepSleep(0, false);
  uint8_t* buf = (uint8_t*)heap_caps_malloc(Canvas::kBytes, MALLOC_CAP_8BIT);
  if (!buf) {
    Serial.println("framebuffer alloc failed");
  } else {
    int mv = board::batteryMillivolts();
    MaintInfo m;
    m.version = FIRMWARE_VERSION;
    m.batteryMv = mv;
    m.batteryPct = batteryPercent(mv);
    m.updateText = "-";
    Canvas c(buf);
    renderMaintenance(c, m);
    Epd().show(buf);
    free(buf);
  }
  board::deepSleep(0, false);
}

#include <Arduino.h>
#include <stdlib.h>
#include "app.h"
#include "board.h"
#include "core.h"
#include "battery.h"
#include "canvas.h"
#include "epd_ssd2683.h"
#include "text.h"

static const char* wakeName(board::Wake w) {
  switch (w) {
    case board::Wake::PowerOn: return "PowerOn";
    case board::Wake::Timer: return "Timer";
    case board::Wake::ButtonConfirm: return "ButtonConfirm";
    case board::Wake::ButtonDown: return "ButtonDown";
    case board::Wake::Charger: return "Charger";
    default: return "Other";
  }
}

extern "C" bool verifyRollbackLater() { return true; }

void setup() {
  board::begin();
  Serial.begin(115200);
#ifdef DEBUG
  delay(500);
#endif
  Serial.printf("eink-kitchen-panel %s (core %d)\n", FIRMWARE_VERSION, core_version());

  int mv = board::batteryMillivolts();
  Serial.printf("wake=%s battery=%d mV (%d%%) charging=%d\n", wakeName(board::wakeCause()), mv,
                batteryPercent(mv), board::charging());

#ifndef DIAG_TEST_PATTERN
  runWakeCycle();
#endif

#ifdef DIAG_TEST_PATTERN
  uint8_t* buf = (uint8_t*)heap_caps_malloc(Canvas::kBytes, MALLOC_CAP_8BIT);
  if (buf) {
    Canvas c(buf);
    c.fill(Color::White);
    const Color bars[4] = {Color::Black, Color::White, Color::Red, Color::Yellow};
    for (int i = 0; i < 4; i++) c.fillRect(i * 100, 0, 100, 200, bars[i]);
    c.fillRect(0, 200, 400, 100, Color::White);
    drawText(c, 10, 250, Font::Title, "Ä Ö Ü ß Test", Color::Black);
    Epd().show(buf);
    free(buf);
  } else {
    Serial.println("framebuffer alloc failed");
  }
#endif

  board::deepSleep(120, false);
}

void loop() {}

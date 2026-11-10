#pragma once
#include <stdint.h>

// Adafruit-GFX compatible layout. Glyph index = code - first.
// xOffset/yOffset are relative to the cursor on the baseline (yOffset < 0 is above it).
struct GFXglyph {
  uint16_t bitmapOffset;
  uint8_t width, height;
  uint8_t xAdvance;
  int8_t xOffset, yOffset;
};

struct GFXfont {
  const uint8_t* bitmap;
  const GFXglyph* glyph;
  uint16_t first, last;
  uint8_t yAdvance;  // line height
  uint8_t ascent;
};

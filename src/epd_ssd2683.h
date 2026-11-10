#pragma once
#include <stdint.h>

class Epd {
 public:
  // Full refresh from a 30000-byte 2bpp buffer; powers the panel off afterwards.
  void show(const uint8_t* buf30000);
};

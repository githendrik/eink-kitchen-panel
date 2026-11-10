#pragma once
#include <stdint.h>

enum class Color : uint8_t { Black = 0, White = 1, Yellow = 2, Red = 3 };

class Canvas {
 public:
  static constexpr int W = 400, H = 300, kBytes = 30000;
  explicit Canvas(uint8_t* buf) : buf_(buf) {}
  void fill(Color c);
  void pixel(int x, int y, Color c);
  Color get(int x, int y) const;
  void fillRect(int x, int y, int w, int h, Color c);
  void rect(int x, int y, int w, int h, Color c);
  void hline(int x, int y, int w, Color c);
  const uint8_t* data() const { return buf_; }

 private:
  uint8_t* buf_;
};

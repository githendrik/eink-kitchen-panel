#include "canvas.h"

void Canvas::fill(Color c) {
  uint8_t v = static_cast<uint8_t>(c);
  uint8_t b = v | (v << 2) | (v << 4) | (v << 6);
  for (int i = 0; i < kBytes; i++) buf_[i] = b;
}

void Canvas::pixel(int x, int y, Color c) {
  if (x < 0 || x >= W || y < 0 || y >= H) return;
  int idx = y * (W / 4) + x / 4;
  int shift = 6 - 2 * (x % 4);
  buf_[idx] = (buf_[idx] & ~(0x3 << shift)) | (static_cast<uint8_t>(c) << shift);
}

Color Canvas::get(int x, int y) const {
  if (x < 0 || x >= W || y < 0 || y >= H) return Color::White;
  int idx = y * (W / 4) + x / 4;
  int shift = 6 - 2 * (x % 4);
  return static_cast<Color>((buf_[idx] >> shift) & 0x3);
}

void Canvas::fillRect(int x, int y, int w, int h, Color c) {
  int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
  int x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;
  for (int j = y0; j < y1; j++)
    for (int i = x0; i < x1; i++) pixel(i, j, c);
}

void Canvas::hline(int x, int y, int w, Color c) { fillRect(x, y, w, 1, c); }

void Canvas::rect(int x, int y, int w, int h, Color c) {
  if (w <= 0 || h <= 0) return;
  fillRect(x, y, w, 1, c);
  fillRect(x, y + h - 1, w, 1, c);
  fillRect(x, y, 1, h, c);
  fillRect(x + w - 1, y, 1, h, c);
}

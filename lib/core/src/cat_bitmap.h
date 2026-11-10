#pragma once
#include "canvas.h"

// Sleeping cat, roughly 64x40 px, drawn procedurally: yellow body, black outline.
namespace cat {
constexpr int W = 64, H = 40;

inline void ellipse(Canvas& c, int cx, int cy, int a, int b, Color col) {
    for (int y = -b; y <= b; y++)
        for (int x = -a; x <= a; x++)
            if (x * x * b * b + y * y * a * a <= a * a * b * b) c.pixel(cx + x, cy + y, col);
}

inline void blob(Canvas& c, int cx, int cy, int a, int b) {
    ellipse(c, cx, cy, a, b, Color::Black);
    ellipse(c, cx, cy, a - 2, b - 2, Color::Yellow);
}

inline void triangle(Canvas& c, int x0, int y0, int x1, int y1, int x2, int y2, Color col) {
    int minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    int maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    for (int y = miny; y <= maxy; y++)
        for (int x = minx; x <= maxx; x++) {
            long d0 = (long)(x1 - x0) * (y - y0) - (long)(y1 - y0) * (x - x0);
            long d1 = (long)(x2 - x1) * (y - y1) - (long)(y2 - y1) * (x - x1);
            long d2 = (long)(x0 - x2) * (y - y2) - (long)(y0 - y2) * (x - x2);
            bool neg = d0 < 0 || d1 < 0 || d2 < 0, pos = d0 > 0 || d1 > 0 || d2 > 0;
            if (!(neg && pos)) c.pixel(x, y, col);
        }
}

// (x, y) is the top-left of the 64x40 box.
inline void draw(Canvas& c, int x, int y) {
    blob(c, x + 28, y + 28, 28, 11);   // body
    blob(c, x + 57, y + 33, 6, 5);     // tail tip curling round
    blob(c, x + 14, y + 25, 12, 10);   // head
    triangle(c, x + 4, y + 20, x + 5, y + 6, x + 13, y + 16, Color::Black);   // ears
    triangle(c, x + 15, y + 16, x + 23, y + 6, x + 24, y + 21, Color::Black);
    triangle(c, x + 7, y + 17, x + 7, y + 10, x + 12, y + 16, Color::Yellow);
    triangle(c, x + 17, y + 16, x + 21, y + 10, x + 21, y + 18, Color::Yellow);
    c.hline(x + 6, y + 26, 5, Color::Black);   // closed eyes
    c.hline(x + 17, y + 26, 5, Color::Black);
    c.pixel(x + 5, y + 25, Color::Black);
    c.pixel(x + 11, y + 25, Color::Black);
    c.pixel(x + 16, y + 25, Color::Black);
    c.pixel(x + 22, y + 25, Color::Black);
    c.fillRect(x + 13, y + 29, 3, 2, Color::Black);  // nose
}
}  // namespace cat

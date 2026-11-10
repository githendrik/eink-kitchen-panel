#include "text.h"

#include "fonts/font_body.h"
#include "fonts/font_bodybold.h"
#include "fonts/font_huge.h"
#include "fonts/font_small.h"
#include "fonts/font_title.h"

static const GFXfont& fontFor(Font f) {
  switch (f) {
    case Font::Small: return fontSmall;
    case Font::Body: return fontBody;
    case Font::BodyBold: return fontBodyBold;
    case Font::Title: return fontTitle;
    default: return fontHuge;
  }
}

// Decodes one UTF-8 sequence to a font code (Latin-1; U+2026 -> 0x7F; else '?'). Advances p.
static int nextCode(const char*& p) {
  const uint8_t* s = reinterpret_cast<const uint8_t*>(p);
  uint32_t cp;
  int len;
  if (s[0] < 0x80) { cp = s[0]; len = 1; }
  else if ((s[0] & 0xE0) == 0xC0) { cp = s[0] & 0x1F; len = 2; }
  else if ((s[0] & 0xF0) == 0xE0) { cp = s[0] & 0x0F; len = 3; }
  else if ((s[0] & 0xF8) == 0xF0) { cp = s[0] & 0x07; len = 4; }
  else { p++; return '?'; }
  int i = 1;
  for (; i < len; i++) {
    if ((s[i] & 0xC0) != 0x80) break;
    cp = (cp << 6) | (s[i] & 0x3F);
  }
  p += i;
  if (i < len) return '?';
  if (cp == 0x2026) return 0x7F;
  if (cp < 0x20 || (cp > 0x7E && cp < 0xA0) || cp > 0xFF) return '?';
  return static_cast<int>(cp);
}

static const GFXglyph& glyphFor(const GFXfont& f, int code) { return f.glyph[code - f.first]; }

int textWidth(Font fnt, const char* utf8) {
  const GFXfont& f = fontFor(fnt);
  int w = 0;
  const char* p = utf8;
  while (*p) w += glyphFor(f, nextCode(p)).xAdvance;
  return w;
}

static void drawGlyph(Canvas& c, int x, int y, const GFXfont& f, const GFXglyph& g, Color col) {
  const uint8_t* bm = f.bitmap + g.bitmapOffset;
  int bit = 0;
  for (int j = 0; j < g.height; j++)
    for (int i = 0; i < g.width; i++, bit++)
      if (bm[bit >> 3] & (0x80 >> (bit & 7))) c.pixel(x + g.xOffset + i, y + g.yOffset + j, col);
}

int drawText(Canvas& c, int x, int y, Font fnt, const char* utf8, Color col, int maxW) {
  const GFXfont& f = fontFor(fnt);
  int ellW = glyphFor(f, 0x7F).xAdvance;
  int total = maxW > 0 ? textWidth(fnt, utf8) : 0;
  bool truncate = maxW > 0 && total > maxW;
  int limit = truncate ? maxW - ellW : 0;
  int cx = 0;
  const char* p = utf8;
  while (*p) {
    int code = nextCode(p);
    const GFXglyph& g = glyphFor(f, code);
    if (truncate && cx + g.xAdvance > limit) break;
    drawGlyph(c, x + cx, y, f, g, col);
    cx += g.xAdvance;
  }
  if (truncate) {
    drawGlyph(c, x + cx, y, f, glyphFor(f, 0x7F), col);
    cx += ellW;
  }
  return cx;
}

int fontLineHeight(Font f) { return fontFor(f).yAdvance; }

#include <unity.h>
#include "canvas.h"
#include "text.h"

static uint8_t buf[Canvas::kBytes];

void setUp() {}
void tearDown() {}

void test_fill_white_bytes() {
  Canvas c(buf);
  c.fill(Color::White);
  for (int i = 0; i < Canvas::kBytes; i++) TEST_ASSERT_EQUAL_HEX8(0x55, c.data()[i]);
}

void test_pixel_packing() {
  Canvas c(buf);
  c.fill(Color::White);
  c.pixel(0, 0, Color::Red);
  TEST_ASSERT_EQUAL_HEX8(0xD5, c.data()[0]);
  c.pixel(3, 0, Color::Yellow);
  TEST_ASSERT_EQUAL_HEX8(0xD6, c.data()[0]);
  c.pixel(4, 0, Color::Black);
  TEST_ASSERT_EQUAL_HEX8(0x15, c.data()[1]);
}

void test_pixel_packing_yellow_on_white() {
  Canvas c(buf);
  c.fill(Color::White);
  c.pixel(3, 0, Color::Yellow);
  TEST_ASSERT_EQUAL_HEX8(0x56, c.data()[0]);
}

void test_clip() {
  Canvas c(buf);
  c.fill(Color::White);
  c.pixel(-1, 0, Color::Black);
  c.pixel(400, 0, Color::Black);
  c.pixel(0, 300, Color::Black);
  c.pixel(0, -1, Color::Black);
  for (int i = 0; i < Canvas::kBytes; i++) TEST_ASSERT_EQUAL_HEX8(0x55, c.data()[i]);
  TEST_ASSERT_EQUAL(Color::White, c.get(-1, 0));
}

void test_get_roundtrip() {
  Canvas c(buf);
  c.fill(Color::White);
  Color cols[] = {Color::Black, Color::White, Color::Yellow, Color::Red};
  for (int i = 0; i < 8; i++) c.pixel(i + 5, 7, cols[i % 4]);
  for (int i = 0; i < 8; i++) TEST_ASSERT_EQUAL(cols[i % 4], c.get(i + 5, 7));
}

void test_rects() {
  Canvas c(buf);
  c.fill(Color::White);
  c.rect(10, 10, 5, 4, Color::Black);
  TEST_ASSERT_EQUAL(Color::Black, c.get(10, 10));
  TEST_ASSERT_EQUAL(Color::Black, c.get(14, 13));
  TEST_ASSERT_EQUAL(Color::White, c.get(12, 11));
  c.fillRect(20, 20, 3, 3, Color::Red);
  TEST_ASSERT_EQUAL(Color::Red, c.get(22, 22));
  TEST_ASSERT_EQUAL(Color::White, c.get(23, 22));
  c.hline(0, 50, 400, Color::Yellow);
  TEST_ASSERT_EQUAL(Color::Yellow, c.get(399, 50));
}

void test_text_umlaut_width() {
  int w = textWidth(Font::Body, "Ä");
  TEST_ASSERT_GREATER_THAN(0, w);
  TEST_ASSERT_EQUAL(w, textWidth(Font::Body, "\xC3\x84"));
  TEST_ASSERT_NOT_EQUAL(textWidth(Font::Body, "A"), 0);
}

void test_text_unknown_glyph() {
  TEST_ASSERT_EQUAL(textWidth(Font::Body, "?"), textWidth(Font::Body, "\xF0\x9F\x98\x80"));
}

void test_truncate() {
  Canvas c(buf);
  c.fill(Color::White);
  const char* s = "Ein sehr langer Termin der nicht passt";
  int w = drawText(c, 0, 20, Font::Body, s, Color::Black, 100);
  TEST_ASSERT_LESS_OR_EQUAL(100, w);
  TEST_ASSERT_GREATER_THAN(50, w);
  int ew = textWidth(Font::Body, "\xE2\x80\xA6");
  TEST_ASSERT_GREATER_THAN(0, ew);
  bool ink = false;
  for (int y = 0; y < 30; y++)
    for (int x = w - ew; x < w; x++)
      if (c.get(x, y) == Color::Black) ink = true;
  TEST_ASSERT_TRUE(ink);
  for (int y = 0; y < 30; y++)
    for (int x = 100; x < 400; x++) TEST_ASSERT_EQUAL(Color::White, c.get(x, y));
}

void test_text_draws_pixels() {
  Canvas c(buf);
  c.fill(Color::White);
  int w = drawText(c, 5, 30, Font::Body, "Heute", Color::Black);
  TEST_ASSERT_GREATER_THAN(0, w);
  int n = 0;
  for (int y = 0; y < 40; y++)
    for (int x = 5; x < 5 + w; x++)
      if (c.get(x, y) == Color::Black) n++;
  TEST_ASSERT_GREATER_THAN(10, n);
  TEST_ASSERT_GREATER_THAN(0, fontLineHeight(Font::Huge));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fill_white_bytes);
  RUN_TEST(test_pixel_packing);
  RUN_TEST(test_pixel_packing_yellow_on_white);
  RUN_TEST(test_clip);
  RUN_TEST(test_get_roundtrip);
  RUN_TEST(test_rects);
  RUN_TEST(test_text_umlaut_width);
  RUN_TEST(test_text_unknown_glyph);
  RUN_TEST(test_truncate);
  RUN_TEST(test_text_draws_pixels);
  return UNITY_END();
}

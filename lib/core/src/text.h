#pragma once
#include "canvas.h"

enum class Font { Small, Body, BodyBold, Title, Huge };

int textWidth(Font f, const char* utf8);
// Baseline at y; truncates with "…" if wider than maxW (maxW<=0: no limit). Returns drawn width.
int drawText(Canvas& c, int x, int y, Font f, const char* utf8, Color col, int maxW = 0);
int fontLineHeight(Font f);

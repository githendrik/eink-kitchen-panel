#include "render.h"

#include <cstdio>

#include "cat_bitmap.h"
#include "text.h"

namespace {
constexpr int kMargin = 12;
constexpr int kMaxToday = 5;
constexpr int kBannerH = 92;  // top of the calendar area (24 px warning bar + 68 px compact banner = same)
constexpr int kCalendarBottom = 282;  // last baseline above the footer (updated time at 296)

// 12x12 arrow; dir 0=right 1=left 2=up 3=down. (x, y) is the top-left.
void arrow(Canvas& c, int x, int y, int dir, Color col) {
    for (int u = 0; u < 12; u++) {
        int half = u < 6 ? 1 : 6 - (u - 6);  // shaft 2 px thick, head tapers
        for (int v = 6 - half; v < 6 + half; v++) {
            int px, py;
            switch (dir) {
                case 0: px = u; py = v; break;
                case 1: px = 11 - u; py = v; break;
                case 2: px = v; py = 11 - u; break;
                default: px = v; py = u; break;
            }
            c.pixel(x + px, y + py, col);
        }
    }
}

int center(int w) { return (Canvas::W - w) / 2; }

void centered(Canvas& c, int y, Font f, const char* s, Color col) {
    drawText(c, center(textWidth(f, s)), y, f, s, col);
}

// Draws text, returns x after it.
int put(Canvas& c, int x, int y, Font f, const std::string& s, Color col, int maxW = 0) {
    return x + drawText(c, x, y, f, s.c_str(), col, maxW);
}

void splitUnit(const std::string& s, std::string& num, std::string& unit) {
    size_t p = s.find(' ');
    num = s.substr(0, p);
    unit = p == std::string::npos ? "" : s.substr(p + 1);
}

// "Netz → 1.1 kW": draws label, arrow shape, value.
void drawGrid(Canvas& c, int x, int y, const std::string& text, Color col) {
    int dir = -1;
    size_t at = std::string::npos;
    const char* arrows[] = {"→", "←"};
    for (int i = 0; i < 2; i++) {
        size_t p = text.find(arrows[i]);
        if (p != std::string::npos) {
            at = p;
            dir = i;
        }
    }
    if (at == std::string::npos) {
        put(c, x, y, Font::Body, text, col);
        return;
    }
    std::string pre = text.substr(0, at), post = text.substr(at + 3);
    x = put(c, x, y, Font::Body, pre, col);
    arrow(c, x, y - 11, dir, col);
    x += 12;
    put(c, x, y, Font::Body, post, col);
}

void drawBanner(Canvas& c, const View& v, int top, bool compact) {
    int bh = compact ? 68 : kBannerH;
    c.fillRect(0, top, Canvas::W, bh, Color::Yellow);
    c.fillRect(0, top + bh - 3, Canvas::W, 3, Color::Black);

    drawText(c, kMargin, top + (compact ? 15 : 20), Font::BodyBold, "Solar", Color::Black);
    int numBase = top + (compact ? 58 : 68);
    if (v.solarMissing) {
        drawText(c, kMargin, numBase - 12, Font::Title, "keine Daten", Color::Red, 370);
        return;
    }
    if (v.solarStale)
        drawText(c, kMargin + 60, top + (compact ? 15 : 20), Font::Small, v.solarStandText.c_str(), Color::Red, 120);
    std::string num, unit;
    splitUnit(v.pvText, num, unit);
    int x = put(c, kMargin, numBase, Font::Huge, num, Color::Black, 150);
    put(c, x + 4, numBase, Font::Title, unit, Color::Black);

    const int rx = 205;
    int p = compact ? 17 : 20;
    int b1 = top + (compact ? 15 : 22);
    put(c, rx, b1, Font::Body, v.houseText, Color::Black, 190);
    drawGrid(c, rx, b1 + p, v.gridText, v.warnGridImport ? Color::Red : Color::Black);
    Color bc = v.warnHomeBatteryLow ? Color::Red : Color::Black;
    char buf[24];
    snprintf(buf, sizeof buf, "Akku %d%%", v.batterySoc);
    int ax = put(c, rx, b1 + 2 * p, Font::Body, buf, bc);
    if (v.batteryArrow == "↑") arrow(c, ax + 6, b1 + 2 * p - 11, 2, bc);
    if (v.batteryArrow == "↓") arrow(c, ax + 6, b1 + 2 * p - 11, 3, bc);
    int by = b1 + 2 * p + 5, bw = 180;
    c.rect(rx, by, bw, 12, Color::Black);
    c.rect(rx + 1, by + 1, bw - 2, 10, Color::Black);
    int soc = v.batterySoc < 0 ? 0 : (v.batterySoc > 100 ? 100 : v.batterySoc);
    c.fillRect(rx + 2, by + 2, (bw - 4) * soc / 100, 8, bc);
}

// Draws event lines starting at baseline y; returns y after the last line.
int drawToday(Canvas& c, const View& v, int y, int maxLines) {
    if (v.today.empty()) {
        drawText(c, kMargin, y, Font::Body, "keine Termine", Color::Black);
        return y + 19;
    }
    int n = visibleTodayLines(v);
    for (int i = 0; i < n && i < maxLines; i++) {
        const EventLine& e = v.today[i];
        drawText(c, kMargin, y, Font::BodyBold, e.time.c_str(), Color::Black, 100);
        drawText(c, kMargin + 106, y, Font::Body, e.title.c_str(), Color::Black, Canvas::W - kMargin - 106 - kMargin);
        y += 19;
    }
    if (static_cast<int>(v.today.size()) > kMaxToday) {
        char b[24];
        snprintf(b, sizeof b, "+%d weitere", static_cast<int>(v.today.size()) - n);
        drawText(c, kMargin + 106, y, Font::Body, b, Color::Black);
        y += 19;
    }
    return y;
}

// Tomorrow: one event per line (Small font, time column) while lines fit above
// `bottom` (last baseline); overflow ends with "+N weitere".
void drawTomorrow(Canvas& c, const View& v, int y, int bottom, int pitch) {
    if (v.tomorrow.empty()) {
        drawText(c, kMargin, y, Font::Small, "keine Termine", Color::Black);
        return;
    }
    const int titleX = kMargin + 80;
    const int titleW = Canvas::W - titleX - kMargin;
    int fit = (bottom - y) / pitch + 1;
    if (fit < 1) fit = 1;
    int n = static_cast<int>(v.tomorrow.size());
    int shown = n <= fit ? n : (fit > 1 ? fit - 1 : 1);
    bool suffix = shown < n && shown == fit;  // no spare line: append "+N" to the last title
    for (int i = 0; i < shown; i++) {
        const EventLine& e = v.tomorrow[i];
        std::string title = e.title;
        if (suffix && i == shown - 1) title += " (+" + std::to_string(n - shown) + ")";
        drawText(c, kMargin, y, Font::Small, e.time.c_str(), Color::Black, titleX - kMargin - 6);
        drawText(c, titleX, y, Font::Small, title.c_str(), Color::Black, titleW);
        y += pitch;
    }
    if (shown < n && !suffix) {
        char b[24];
        snprintf(b, sizeof b, "+%d weitere", n - shown);
        drawText(c, titleX, y, Font::Small, b, Color::Black);
    }
}

void drawCalendar(Canvas& c, const View& v, int top) {
    int hb = top + 26;  // 10 px more air below the banner
    std::string head = v.todayLabel;
    if (!v.headerDate.empty()) head += " · " + v.headerDate;
    drawText(c, kMargin, hb, Font::BodyBold, head.c_str(), Color::Black);
    int y = drawToday(c, v, hb + 21, kMaxToday);
    int sep = y - 19 + 14;
    c.hline(kMargin, sep, Canvas::W - 2 * kMargin, Color::Black);
    drawText(c, kMargin, sep + 21, Font::BodyBold, v.tomorrowLabel.c_str(), Color::Black);
    drawTomorrow(c, v, sep + 39, kCalendarBottom, 15);
}
}  // namespace

int visibleTodayLines(const View& v) {
    int n = static_cast<int>(v.today.size());
    return n > kMaxToday ? kMaxToday - 1 : n;
}

void renderDay(Canvas& c, const View& v) {
    c.fill(Color::White);
    int top = 0;
    if (v.warnPanelBattery) {
        c.fillRect(0, 0, Canvas::W, 24, Color::Red);
        centered(c, 17, Font::BodyBold, "Panel-Akku schwach - bitte laden", Color::White);
        top = 24;
    }
    drawBanner(c, v, top, v.warnPanelBattery);
    drawCalendar(c, v, kBannerH);
    if (v.dataStale)
        put(c, kMargin, 296, Font::Small, "Daten veraltet · " + v.staleText, Color::Red, 250);
    int w = textWidth(Font::Small, v.updatedAt.c_str());
    drawText(c, Canvas::W - kMargin - w, 296, Font::Small, v.updatedAt.c_str(), v.dataStale ? Color::Red : Color::Black);
}

void renderNight(Canvas& c, const View& v) {
    c.fill(Color::White);
    int top = 0;
    if (v.warnPanelBattery) {
        c.fillRect(0, 0, Canvas::W, 24, Color::Red);
        centered(c, 17, Font::BodyBold, "Panel-Akku schwach - bitte laden", Color::White);
        top = 24;
    }
    int bh = v.warnPanelBattery ? 68 : kBannerH;
    c.fillRect(0, top, Canvas::W, bh, Color::Yellow);
    c.fillRect(0, top + bh - 3, Canvas::W, 3, Color::Black);
    int mid = top + (bh - 3) / 2;
    drawText(c, kMargin, mid + 14, Font::Huge, "Gute Nacht", Color::Black, 270);
    cat::draw(c, Canvas::W - kMargin - cat::W, mid - cat::H / 2);
    drawCalendar(c, v, kBannerH);
}

void renderChargeMe(Canvas& c) {
    c.fill(Color::White);
    c.rect(6, 6, Canvas::W - 12, Canvas::H - 12, Color::Red);
    c.rect(7, 7, Canvas::W - 14, Canvas::H - 14, Color::Red);
    c.rect(8, 8, Canvas::W - 16, Canvas::H - 16, Color::Red);
    centered(c, 140, Font::Huge, "Akku leer", Color::Red);
    centered(c, 190, Font::Title, "bitte laden", Color::Red);
}

void renderOff(Canvas& c) {
    c.fill(Color::White);
    c.rect(6, 6, Canvas::W - 12, Canvas::H - 12, Color::Black);
    centered(c, 130, Font::Huge, "Aus", Color::Black);
    centered(c, 180, Font::Body, "Zum Einschalten", Color::Black);
    centered(c, 202, Font::Body, "seitliche Taste unten drücken", Color::Black);
}

void renderMaintenance(Canvas& c, const MaintInfo& m) {
    c.fill(Color::White);
    c.fillRect(0, 0, Canvas::W, 30, Color::Black);
    drawText(c, kMargin, 22, Font::Title, "Wartung", Color::White);
    struct Row {
        const char* k;
        std::string v;
    };
    char bat[40];
    snprintf(bat, sizeof bat, "%d mV (%d%%)", m.batteryMv, m.batteryPct);
    Row rows[] = {{"IP", m.ip},         {"Version", m.version},  {"WLAN", m.ssid},
                  {"Update", m.updateText}, {"Akku", bat},          {"Abruf", m.lastFetch}};
    int y = 58;
    for (const Row& r : rows) {
        drawText(c, kMargin, y, Font::BodyBold, r.k, Color::Black);
        drawText(c, kMargin + 90, y, Font::Body, r.v.c_str(), Color::Black, Canvas::W - kMargin - 90 - kMargin);
        y += 30;
    }
}

// Host simulator: renders scenario JSON files to PNG using the production layouts.
// Usage: sim <scenario-dir> <output-dir>
#include <ArduinoJson.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "canvas.h"
#include "kitchen.h"
#include "render.h"
#include "view.h"

namespace fs = std::filesystem;
static const char* kTz = "CET-1CEST,M3.5.0,M10.5.0/3";

static uint32_t crcTable[256];
static void initCrc() {
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crcTable[n] = c;
    }
}
static uint32_t crc(const uint8_t* d, size_t n, uint32_t c = 0xFFFFFFFFu) {
    for (size_t i = 0; i < n; i++) c = crcTable[(c ^ d[i]) & 0xFF] ^ (c >> 8);
    return c;
}
static void be32(std::vector<uint8_t>& o, uint32_t v) {
    for (int s = 24; s >= 0; s -= 8) o.push_back((v >> s) & 0xFF);
}
static void chunk(std::vector<uint8_t>& o, const char* type, const std::vector<uint8_t>& data) {
    be32(o, data.size());
    std::vector<uint8_t> td(type, type + 4);
    td.insert(td.end(), data.begin(), data.end());
    o.insert(o.end(), td.begin(), td.end());
    be32(o, crc(td.data(), td.size()) ^ 0xFFFFFFFFu);
}

static bool writePng(const std::string& path, const Canvas& cv, int scale) {
    static const uint8_t pal[4][3] = {{0, 0, 0}, {255, 255, 255}, {0xf0, 0xc0, 0x00}, {0xc0, 0, 0}};
    int w = Canvas::W * scale, h = Canvas::H * scale;
    std::vector<uint8_t> raw;
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        for (int x = 0; x < w; x++) {
            const uint8_t* p = pal[static_cast<int>(cv.get(x / scale, y / scale))];
            raw.insert(raw.end(), p, p + 3);
        }
    }
    std::vector<uint8_t> z = {0x78, 0x01};
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t n = std::min<size_t>(65535, raw.size() - pos);
        bool last = pos + n == raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(n & 0xFF);
        z.push_back(n >> 8);
        z.push_back(~n & 0xFF);
        z.push_back((~n >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    be32(z, (b << 16) | a);

    std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr;
    be32(ihdr, w);
    be32(ihdr, h);
    for (uint8_t v : {8, 2, 0, 0, 0}) ihdr.push_back(v);
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(out.data()), out.size());
    return f.good();
}

static int64_t isoOr(JsonVariantConst v, int64_t def) {
    int64_t t;
    return v.is<const char*>() && parseIsoUtc(v.as<const char*>(), t) ? t : def;
}

static bool runScenario(const fs::path& file, const fs::path& outDir) {
    std::ifstream in(file);
    std::stringstream ss;
    ss << in.rdbuf();
    std::string text = ss.str();
    JsonDocument doc;
    if (deserializeJson(doc, text)) {
        fprintf(stderr, "%s: bad JSON\n", file.c_str());
        return false;
    }
    std::string screen = doc["screen"] | "day";
    int64_t now = isoOr(doc["now"], 0);
    int pct = doc["panelBatteryPct"] | 100;
    int64_t last = isoOr(doc["lastSuccess"], 0);

    Kitchen k;
    std::string kjson, err;
    serializeJson(doc["kitchen"], kjson);
    if (screen == "day" || screen == "night") {
        if (!parseKitchen(kjson.data(), kjson.size(), k, err)) {
            fprintf(stderr, "%s: kitchen: %s\n", file.c_str(), err.c_str());
            return false;
        }
    }
    static uint8_t buf[Canvas::kBytes];
    Canvas c(buf);
    if (screen == "day") {
        renderDay(c, buildDayView(k, now, kTz, pct, last));
    } else if (screen == "night") {
        renderNight(c, buildNightView(k, now, kTz, pct));
    } else if (screen == "charge_me") {
        renderChargeMe(c);
    } else if (screen == "off") {
        renderOff(c);
    } else if (screen == "maintenance") {
        JsonObjectConst m = doc["maint"];
        MaintInfo mi;
        mi.ip = m["ip"] | "";
        mi.version = m["version"] | "";
        mi.ssid = m["ssid"] | "";
        mi.updateText = m["updateText"] | "";
        mi.batteryMv = m["batteryMv"] | 0;
        mi.batteryPct = m["batteryPct"] | 0;
        mi.lastFetch = m["lastFetch"] | "";
        renderMaintenance(c, mi);
    } else {
        fprintf(stderr, "%s: unknown screen\n", file.c_str());
        return false;
    }
    fs::path out = outDir / (file.stem().string() + ".png");
    printf("%s\n", out.c_str());
    return writePng(out.string(), c, 1);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: sim <scenario-dir> <out-dir>\n");
        return 2;
    }
    initCrc();
    fs::create_directories(argv[2]);
    std::vector<fs::path> files;
    for (auto& e : fs::directory_iterator(argv[1]))
        if (e.path().extension() == ".json") files.push_back(e.path());
    std::sort(files.begin(), files.end());
    int bad = 0;
    for (auto& f : files) bad += !runScenario(f, argv[2]);
    return bad ? 1 : 0;
}

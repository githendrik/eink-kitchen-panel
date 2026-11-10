#include "kitchen.h"

#include <ArduinoJson.h>

#include <cstdio>

namespace {
int64_t daysFromCivil(int64_t y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

void readDay(JsonObjectConst src, Day& out) {
    out.date = src["date"] | "";
    out.events.clear();
    for (JsonObjectConst e : src["events"].as<JsonArrayConst>()) {
        Event ev;
        ev.summary = e["summary"] | "";
        ev.start = e["start"] | "";
        ev.end = e["end"] | "";
        ev.allDay = e["allDay"] | false;
        out.events.push_back(std::move(ev));
    }
}
}  // namespace

bool parseIsoUtc(const char* s, int64_t& out) {
    if (!s) return false;
    int y, mo, d, h, mi, sec, n = 0;
    if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d%n", &y, &mo, &d, &h, &mi, &sec, &n) != 6) return false;
    const char* p = s + n;
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9') ++p;
    }
    int64_t off = 0;
    if (*p == 'Z') {
    } else if (*p == '+' || *p == '-') {
        int oh, om;
        if (sscanf(p + 1, "%2d:%2d", &oh, &om) != 2) return false;
        off = (oh * 3600 + om * 60) * (*p == '+' ? 1 : -1);
    } else {
        return false;
    }
    return out = daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + sec - off, true;
}

bool parseKitchen(const char* json, size_t len, Kitchen& out, std::string& err) {
    JsonDocument doc;
    DeserializationError de = deserializeJson(doc, json, len);
    if (de) {
        err = std::string("invalid JSON: ") + de.c_str();
        return false;
    }
    JsonObjectConst root = doc.as<JsonObjectConst>();
    JsonObjectConst cal = root["calendar"];
    if (cal.isNull() || cal["today"].isNull() || cal["tomorrow"].isNull()) {
        err = "missing calendar";
        return false;
    }
    Kitchen k;
    readDay(cal["today"], k.today);
    readDay(cal["tomorrow"], k.tomorrow);
    JsonObjectConst s = root["solar"];
    if (!s.isNull()) {
        Solar so;
        so.pvW = s["pv_w"] | 0;
        so.loadW = s["load_w"] | 0;
        so.gridW = s["grid_w"] | 0;
        so.batterySoc = s["battery_soc"] | 0;
        so.batteryW = s["battery_w"] | 0;
        so.stale = s["stale"] | false;
        if (!parseIsoUtc(s["updated_at"] | "", so.updatedAtUtc)) {
            so.updatedAtUtc = 0;
            so.stale = true;
        }
        k.solar = so;
    }
    if (!parseIsoUtc(root["server_time"] | "", k.serverTimeUtc)) k.serverTimeUtc = 0;
    out = std::move(k);
    return true;
}

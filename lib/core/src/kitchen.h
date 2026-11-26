#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct Event {
    std::string summary;
    std::string start;
    std::string end;
    bool allDay = false;
};
struct Day {
    std::string date;
    std::vector<Event> events;
};
struct Solar {
    int pvW = 0, loadW = 0, gridW = 0, batterySoc = 0, batteryW = 0;
    int64_t updatedAtUtc = 0;
    bool stale = false;
};
constexpr int16_t kNoData = -1;
// Today's production/consumption in bins of stepMin minutes from startMin
// (minutes after local midnight); values in units of unitW, kNoData = no data.
struct History {
    std::string date;  // local YYYY-MM-DD
    int startMin = 360, stepMin = 5, unitW = 100;
    std::vector<int16_t> pv, load;
    int pvWh = 0, loadWh = 0;
};
struct Kitchen {
    Day today, tomorrow;
    std::optional<Solar> solar;
    std::optional<History> history;
    int64_t serverTimeUtc = 0;
};

bool parseKitchen(const char* json, size_t len, Kitchen& out, std::string& err);

// Parses "YYYY-MM-DDTHH:MM:SS[.fff](Z|+HH:MM|-HH:MM)" to epoch seconds UTC.
bool parseIsoUtc(const char* s, int64_t& out);

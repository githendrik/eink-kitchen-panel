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
struct Kitchen {
    Day today, tomorrow;
    std::optional<Solar> solar;
    int64_t serverTimeUtc = 0;
};

bool parseKitchen(const char* json, size_t len, Kitchen& out, std::string& err);

// Parses "YYYY-MM-DDTHH:MM:SS[.fff](Z|+HH:MM|-HH:MM)" to epoch seconds UTC.
bool parseIsoUtc(const char* s, int64_t& out);

#pragma once
#include <string>

#include "canvas.h"
#include "view.h"

struct MaintInfo {
    std::string ip, version, ssid, updateText;
    int batteryMv = 0, batteryPct = 0;
    std::string lastFetch;
};

// Number of real event lines drawn for today (max 5; when more than 5 events,
// 4 are shown and the 5th line reads "+N weitere").
int visibleTodayLines(const View& v);

void renderDay(Canvas& c, const View& v);
void renderNight(Canvas& c, const View& v);
void renderChargeMe(Canvas& c);
void renderOff(Canvas& c);  // standby: "Aus", how to switch back on
void renderMaintenance(Canvas& c, const MaintInfo& m);

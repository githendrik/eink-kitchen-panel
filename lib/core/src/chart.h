#pragma once
#include <vector>

#include "view.h"

// One pixel column of the day chart; heights in pixels above the baseline.
struct ChartColumn {
    bool hasData = false;
    int pvH = 0, loadH = 0;
};

// Maps 06:00-22:00 onto plotW columns. A column has data when it lies at or
// before "now", within the bins, and its own bin has data for both series.
// Values are interpolated linearly between neighbouring bin centres.
std::vector<ChartColumn> chartColumns(const ChartView& c, int plotW, int plotH);

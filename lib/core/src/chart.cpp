#include "chart.h"

#include <cmath>

namespace {
constexpr double kStartMin = 6 * 60, kSpanMin = 16 * 60;

// Value at minute t for bins of `step` minutes from `start`; -1 if the bin t
// falls into has no data. Interpolates towards the neighbour on t's side of
// the bin centre when that neighbour has data.
double valueAt(const std::vector<int>& w, int start, int step, double t) {
    int n = static_cast<int>(w.size());
    int own = static_cast<int>(std::floor((t - start) / step));
    if (own < 0 || own >= n || w[own] < 0) return -1;
    double p = (t - start) / step - 0.5;
    int i0 = static_cast<int>(std::floor(p)), i1 = i0 + 1;
    double f = p - i0;
    bool ok0 = i0 >= 0 && w[i0] >= 0, ok1 = i1 < n && w[i1] >= 0;
    if (ok0 && ok1) return w[i0] + (w[i1] - w[i0]) * f;
    return w[own];
}

int height(double w, int scaleKw, int plotH) {
    int h = static_cast<int>(std::lround(w / (scaleKw * 1000.0) * plotH));
    return h < 0 ? 0 : (h > plotH ? plotH : h);
}
}  // namespace

std::vector<ChartColumn> chartColumns(const ChartView& c, int plotW, int plotH) {
    std::vector<ChartColumn> cols(plotW > 0 ? plotW : 0);
    int scale = c.scaleKw > 0 ? c.scaleKw : 1;
    for (int x = 0; x < plotW; x++) {
        double t = kStartMin + (x + 0.5) * kSpanMin / plotW;
        if (t > c.nowMin) break;
        double pv = valueAt(c.pvW, c.startMin, c.stepMin, t);
        double load = valueAt(c.loadW, c.startMin, c.stepMin, t);
        if (pv < 0 || load < 0) continue;
        cols[x] = {true, height(pv, scale, plotH), height(load, scale, plotH)};
    }
    return cols;
}

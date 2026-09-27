#include "stroke.h"
#include <limits>

namespace dreams {

void Stroke::addPoint(const DrawPoint& p) {
    if (finished_) return; // traço fechado: ignora silenciosamente (defensivo em runtime de UI)
    points_.push_back(p);
}

void Stroke::boundingBox(float out[4]) const {
    if (points_.empty()) { out[0] = out[1] = out[2] = out[3] = 0.f; return; }
    float minX = std::numeric_limits<float>::max(), minY = minX;
    float maxX = -minX, maxY = -minX;
    const float pad = brush_.baseSizePx;
    for (const auto& p : points_) {
        minX = std::min(minX, p.x - pad);
        minY = std::min(minY, p.y - pad);
        maxX = std::max(maxX, p.x + pad);
        maxY = std::max(maxY, p.y + pad);
    }
    out[0] = minX; out[1] = minY; out[2] = maxX; out[3] = maxY;
}

} // namespace dreams

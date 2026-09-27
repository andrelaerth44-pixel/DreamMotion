#pragma once
#include <vector>
#include <cstdint>
#include "engine_types.h"

namespace dreams {

// Um traço é a unidade de desenho: a sequência de pontos capturados durante um
// gesto (dedo/caneta para baixo → solto), associada ao pincel usado. Fica "vivo"
// (re-renderizável) até a camada ser achatada — permite ajustar sem perder qualidade.
class Stroke {
public:
    Stroke(uint64_t id, Brush brush, uint32_t colorArgb)
        : id_(id), brush_(brush), colorArgb_(colorArgb) {}

    void addPoint(const DrawPoint& p);
    void finish() { finished_ = true; }
    bool isFinished() const { return finished_; }

    const std::vector<DrawPoint>& points() const { return points_; }
    const Brush& brush() const { return brush_; }
    uint32_t color() const { return colorArgb_; }
    uint64_t id() const { return id_; }

    // Preenche out[0..3] = minX, minY, maxX, maxY em coordenadas do canvas,
    // já com a margem do pincel — usado para invalidar só a região suja.
    void boundingBox(float out[4]) const;

private:
    uint64_t id_;
    Brush brush_;
    uint32_t colorArgb_;
    std::vector<DrawPoint> points_;
    bool finished_ = false;
};

} // namespace dreams

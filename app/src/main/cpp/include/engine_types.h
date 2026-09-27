#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace dreams {

enum class BlendMode : uint8_t { Normal, Multiply, Screen, Add, Erase };

struct DrawPoint {
    float x = 0.f;
    float y = 0.f;
    float pressure = 1.f;
    float tiltX = 0.f;
    float tiltY = 0.f;
    int64_t timestampNanos = 0;
};

// Parâmetros de um pincel. Cada DrawPoint recebido é transformado em geometria
// (um "carimbo") pelo GLRenderEngine usando estes valores para variar tamanho/
// opacidade conforme a pressão.
struct Brush {
    float baseSizePx = 24.f;
    float minSizeFactor = 0.2f;   // tamanho mínimo como fração do baseSize (pressão 0)
    float opacity = 1.f;
    float hardness = 0.75f;       // 0 = borda bem difusa, 1 = borda dura
    float spacing = 0.1f;         // distância entre carimbos, fração do tamanho (uso futuro)
    bool pressureAffectsSize = true;
    bool pressureAffectsOpacity = true;
    BlendMode blendMode = BlendMode::Normal;
};

struct Transform {
    float translateX = 0.f, translateY = 0.f;
    float scale = 1.f;
    float rotationDeg = 0.f;
    float opacity = 1.f;

    static Transform lerp(const Transform& a, const Transform& b, float t) {
        return Transform{
            a.translateX + (b.translateX - a.translateX) * t,
            a.translateY + (b.translateY - a.translateY) * t,
            a.scale + (b.scale - a.scale) * t,
            a.rotationDeg + (b.rotationDeg - a.rotationDeg) * t,
            a.opacity + (b.opacity - a.opacity) * t,
        };
    }
};

enum class Easing : uint8_t { Linear, EaseIn, EaseOut, EaseInOut };

inline float applyEasing(Easing e, float t) {
    switch (e) {
        case Easing::EaseIn: return t * t;
        case Easing::EaseOut: { float u = 1.f - t; return 1.f - u * u; }
        case Easing::EaseInOut:
            return t < 0.5f ? 2.f * t * t : 1.f - std::pow(-2.f * t + 2.f, 2.f) / 2.f;
        default: return t;
    }
}

} // namespace dreams

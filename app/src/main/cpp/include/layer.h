#pragma once
#include <vector>
#include <memory>
#include <string>
#include "stroke.h"
#include "engine_types.h"

namespace dreams {

// Uma camada dentro de um Frame. No estado atual do compositing (ver
// GLRenderEngine::drawFrame) os traços ainda são desenhados direto no framebuffer
// padrão; textureHandle/fboHandle já existem na estrutura para quando o
// compositing por camada (FBO próprio + blend mode) for implementado.
class Layer {
public:
    explicit Layer(std::string layerName) : name(std::move(layerName)) {}

    void addStroke(std::unique_ptr<Stroke> stroke) {
        strokes_.push_back(std::move(stroke));
        dirty = true;
    }

    const std::vector<std::unique_ptr<Stroke>>& strokes() const { return strokes_; }

    std::string name;
    bool visible = true;
    bool locked = false;
    float opacity = 1.f;
    BlendMode blendMode = BlendMode::Normal;

    int textureHandle = -1; // -1 = ainda não alocado no backend gráfico
    int fboHandle = -1;
    bool dirty = true;      // conteúdo raster desatualizado em relação a strokes()

private:
    std::vector<std::unique_ptr<Stroke>> strokes_;
};

} // namespace dreams

#pragma once
#include <vector>
#include <memory>
#include <string>
#include "stroke.h"
#include "engine_types.h"

namespace dreams {

// Uma camada dentro de um Frame. Ver GLRenderEngine para como o compositing por
// FBO usa textureHandle/fboHandle. popLastStroke/restoreStroke existem para dar
// suporte a undo/redo no nível de traço (ver GLRenderEngine::undo/redo).
class Layer {
public:
    explicit Layer(std::string layerName) : name(std::move(layerName)) {}

    void addStroke(std::unique_ptr<Stroke> stroke) {
        strokes_.push_back(std::move(stroke));
        dirty = true;
    }

    // Remove e devolve o último traço da camada (para undo). nullptr se vazia.
    std::unique_ptr<Stroke> popLastStroke() {
        if (strokes_.empty()) return nullptr;
        auto s = std::move(strokes_.back());
        strokes_.pop_back();
        dirty = true;
        return s;
    }

    // Devolve um traço previamente removido por popLastStroke (para redo).
    void restoreStroke(std::unique_ptr<Stroke> stroke) {
        strokes_.push_back(std::move(stroke));
        dirty = true;
    }

    const std::vector<std::unique_ptr<Stroke>>& strokes() const { return strokes_; }

    std::string name;
    bool visible = true;
    bool locked = false;
    float opacity = 1.f;
    BlendMode blendMode = BlendMode::Normal;

    int textureHandle = -1;
    int fboHandle = -1;
    bool dirty = true;

private:
    std::vector<std::unique_ptr<Stroke>> strokes_;
};

} // namespace dreams

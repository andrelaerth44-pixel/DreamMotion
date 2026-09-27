#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include "layer.h"
#include "engine_types.h"

namespace dreams {

enum class FrameType : uint8_t { Drawn, Keyframe, Interpolated };

class Frame {
public:
    explicit Frame(FrameType t = FrameType::Drawn) : type(t) {
        layers.push_back(std::make_unique<Layer>("Camada 1"));
    }

    FrameType type;
    int holdDurationTicks = 1;
    Transform transform;
    Easing easing = Easing::Linear;
    std::vector<std::unique_ptr<Layer>> layers;
};

class Timeline {
public:
    explicit Timeline(int framerate = 24) : framerate_(framerate) {
        frames_.push_back(std::make_unique<Frame>());
    }

    int framerate() const { return framerate_; }
    size_t frameCount() const { return frames_.size(); }
    Frame* frameAt(size_t index) const {
        return index < frames_.size() ? frames_[index].get() : nullptr;
    }

    Frame* appendFrame(FrameType type) {
        frames_.push_back(std::make_unique<Frame>(type));
        return frames_.back().get();
    }

    // Versão baseada em tempo contínuo (ticks acumulados via holdDurationTicks).
    // Mantida para um futuro modelo de playback contínuo; o playback atual
    // (GLRenderEngine::advancePlayback) trabalha diretamente em índice de frame
    // e usa resolveTransformForFrameIndex abaixo, mais direto para esse caso.
    Transform resolveTransformAtTick(int64_t tick) const;

    // Resolve o Transform efetivo para um índice de frame específico, interpolando
    // entre o KEYFRAME anterior e o próximo quando o frame é do tipo Interpolated.
    // A fração t é a posição relativa real entre os dois índices de keyframe —
    // substitui o antigo placeholder fixo (t = 0.5).
    Transform resolveTransformForFrameIndex(size_t frameIndex) const;

    // Encontra o frame que efetivamente contém o desenho (camadas) a usar para
    // renderizar o índice pedido: se o frame for Interpolated, caminha para trás
    // até o frame desenhado/keyframe mais próximo. SIMPLIFICAÇÃO DESTA ITERAÇÃO:
    // não existe (ainda) um conceito real de "mesma arte compartilhada entre
    // keyframes" — o conteúdo vem sempre do keyframe anterior mais próximo, e não
    // de um objeto de desenho único referenciado por vários keyframes.
    Frame* contentSourceFrame(size_t frameIndex) const;

private:
    int framerate_;
    std::vector<std::unique_ptr<Frame>> frames_;
};

} // namespace dreams

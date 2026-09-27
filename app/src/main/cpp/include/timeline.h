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

// Gerencia a sequência de frames e resolve, para um tick global, qual transform
// aplicar — incluindo a interpolação entre KEYFRAMEs consecutivos, que é o que
// diferencia uma timeline "Procreate Dreams" de um flipbook simples (FlipaClip).
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

    Transform resolveTransformAtTick(int64_t tick) const;

private:
    int framerate_;
    std::vector<std::unique_ptr<Frame>> frames_;
};

} // namespace dreams

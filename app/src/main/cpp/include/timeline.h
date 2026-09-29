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

    // Remove todos os frames e redefine o framerate. Usado por
    // GLRenderEngine::loadProjectFromFile antes de reconstruir a timeline a
    // partir do arquivo — depois de chamar isto, frameCount() é 0 até o
    // chamador popular via appendFrame novamente.
    void resetEmpty(int framerate) {
        frames_.clear();
        framerate_ = framerate;
    }

    Transform resolveTransformAtTick(int64_t tick) const;
    Transform resolveTransformForFrameIndex(size_t frameIndex) const;
    Frame* contentSourceFrame(size_t frameIndex) const;

private:
    int framerate_;
    std::vector<std::unique_ptr<Frame>> frames_;
};

} // namespace dreams

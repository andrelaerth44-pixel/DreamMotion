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

    // Descarta toda a timeline atual e recomeça com o framerate dado e 1 frame
    // Drawn vazio — usado antes de carregar um projeto salvo (ver project_io.h).
    void reset(int framerate) {
        frames_.clear();
        framerate_ = framerate > 0 ? framerate : 24;
        frames_.push_back(std::make_unique<Frame>());
    }

    Transform resolveTransformAtTick(int64_t tick) const;
    Transform resolveTransformForFrameIndex(size_t frameIndex) const;
    Frame* contentSourceFrame(size_t frameIndex) const;

private:
    int framerate_;
    std::vector<std::unique_ptr<Frame>> frames_;
};

} // namespace dreams

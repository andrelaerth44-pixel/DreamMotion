#include "timeline.h"

namespace dreams {

Transform Timeline::resolveTransformAtTick(int64_t tick) const {
    if (frames_.empty()) return Transform{};
    int64_t acc = 0;
    size_t currentIndex = 0;
    for (size_t i = 0; i < frames_.size(); ++i) {
        acc += frames_[i]->holdDurationTicks;
        currentIndex = i;
        if (tick < acc) break;
    }
    return resolveTransformForFrameIndex(currentIndex);
}

Transform Timeline::resolveTransformForFrameIndex(size_t frameIndex) const {
    if (frameIndex >= frames_.size()) return Transform{};
    const Frame* current = frames_[frameIndex].get();
    if (current->type != FrameType::Interpolated) return current->transform;

    const Frame* prevKey = nullptr;
    const Frame* nextKey = nullptr;
    size_t prevKeyIndex = 0, nextKeyIndex = 0;
    for (size_t i = frameIndex; i-- > 0;) {
        if (frames_[i]->type == FrameType::Keyframe) { prevKey = frames_[i].get(); prevKeyIndex = i; break; }
    }
    for (size_t i = frameIndex + 1; i < frames_.size(); ++i) {
        if (frames_[i]->type == FrameType::Keyframe) { nextKey = frames_[i].get(); nextKeyIndex = i; break; }
    }
    if (!prevKey || !nextKey || nextKeyIndex == prevKeyIndex) return current->transform;

    float t = (float) (frameIndex - prevKeyIndex) / (float) (nextKeyIndex - prevKeyIndex);
    return Transform::lerp(prevKey->transform, nextKey->transform, applyEasing(current->easing, t));
}

Frame* Timeline::contentSourceFrame(size_t frameIndex) const {
    if (frameIndex >= frames_.size()) return nullptr;
    if (frames_[frameIndex]->type != FrameType::Interpolated) return frames_[frameIndex].get();
    for (size_t i = frameIndex; i-- > 0;) {
        if (frames_[i]->type != FrameType::Interpolated) return frames_[i].get();
    }
    return frames_[frameIndex].get();
}

} // namespace dreams

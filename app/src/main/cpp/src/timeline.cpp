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

    const Frame* current = frames_[currentIndex].get();
    if (current->type != FrameType::Interpolated) {
        return current->transform;
    }

    const Frame* prevKey = nullptr;
    const Frame* nextKey = nullptr;
    for (size_t i = currentIndex; i-- > 0;) {
        if (frames_[i]->type == FrameType::Keyframe) { prevKey = frames_[i].get(); break; }
    }
    for (size_t i = currentIndex + 1; i < frames_.size(); ++i) {
        if (frames_[i]->type == FrameType::Keyframe) { nextKey = frames_[i].get(); break; }
    }
    if (!prevKey || !nextKey) return current->transform;

    // TODO: calcular a fração real com base nos ticks acumulados entre prevKey e
    // nextKey (hoje fixo em 0.5 como placeholder).
    float t = 0.5f;
    return Transform::lerp(prevKey->transform, nextKey->transform, applyEasing(current->easing, t));
}

} // namespace dreams

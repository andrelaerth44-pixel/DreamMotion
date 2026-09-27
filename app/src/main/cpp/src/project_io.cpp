#include "project_io.h"
#include <fstream>
#include <cstring>
#include <cstdint>

namespace dreams {
namespace {

constexpr uint32_t kFormatVersion = 1;

template <typename T>
void writeRaw(std::ofstream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool readRaw(std::ifstream& in, T& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return (bool) in;
}

} // namespace

bool saveTimelineToFile(const Timeline& timeline, const std::string& path) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    out.write("TDRM", 4);
    writeRaw(out, kFormatVersion);
    writeRaw(out, (int32_t) timeline.framerate());
    writeRaw(out, (uint32_t) timeline.frameCount());

    for (size_t fi = 0; fi < timeline.frameCount(); ++fi) {
        Frame* frame = timeline.frameAt(fi);
        writeRaw(out, (uint8_t) frame->type);
        writeRaw(out, (int32_t) frame->holdDurationTicks);
        writeRaw(out, frame->transform.translateX);
        writeRaw(out, frame->transform.translateY);
        writeRaw(out, frame->transform.scale);
        writeRaw(out, frame->transform.rotationDeg);
        writeRaw(out, frame->transform.opacity);
        writeRaw(out, (uint8_t) frame->easing);
        writeRaw(out, (uint32_t) frame->layers.size());

        for (auto& layerPtr : frame->layers) {
            Layer& layer = *layerPtr;
            writeRaw(out, (uint32_t) layer.name.size());
            out.write(layer.name.data(), (std::streamsize) layer.name.size());
            writeRaw(out, (uint8_t) (layer.visible ? 1 : 0));
            writeRaw(out, (uint8_t) (layer.locked ? 1 : 0));
            writeRaw(out, layer.opacity);
            writeRaw(out, (uint8_t) layer.blendMode);
            writeRaw(out, (uint32_t) layer.strokes().size());

            for (auto& strokePtr : layer.strokes()) {
                Stroke& stroke = *strokePtr;
                writeRaw(out, stroke.color());
                const Brush& b = stroke.brush();
                writeRaw(out, b.baseSizePx);
                writeRaw(out, b.minSizeFactor);
                writeRaw(out, b.opacity);
                writeRaw(out, b.hardness);
                writeRaw(out, b.spacing);
                writeRaw(out, (uint8_t) (b.pressureAffectsSize ? 1 : 0));
                writeRaw(out, (uint8_t) (b.pressureAffectsOpacity ? 1 : 0));
                writeRaw(out, (uint8_t) b.blendMode);
                writeRaw(out, (uint32_t) stroke.points().size());
                for (const auto& p : stroke.points()) {
                    writeRaw(out, p.x);
                    writeRaw(out, p.y);
                    writeRaw(out, p.pressure);
                    writeRaw(out, p.tiltX);
                    writeRaw(out, p.tiltY);
                    writeRaw(out, p.timestampNanos);
                }
            }
        }
    }
    return (bool) out;
}

bool loadTimelineFromFile(Timeline& timeline, const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    char magic[4];
    in.read(magic, 4);
    if (!in || std::memcmp(magic, "TDRM", 4) != 0) return false;

    uint32_t version;
    if (!readRaw(in, version)) return false;

    int32_t framerate;
    uint32_t frameCount;
    if (!readRaw(in, framerate) || !readRaw(in, frameCount)) return false;

    timeline.reset(framerate);

    for (uint32_t fi = 0; fi < frameCount; ++fi) {
        uint8_t type;
        int32_t hold;
        float tx, ty, scale, rot, op;
        uint8_t easing;
        uint32_t layerCount;
        if (!readRaw(in, type) || !readRaw(in, hold) ||
            !readRaw(in, tx) || !readRaw(in, ty) || !readRaw(in, scale) ||
            !readRaw(in, rot) || !readRaw(in, op) || !readRaw(in, easing) ||
            !readRaw(in, layerCount)) {
            return false;
        }

        Frame* frame = (fi == 0) ? timeline.frameAt(0) : timeline.appendFrame((FrameType) type);
        if (!frame) return false;
        frame->type = (FrameType) type;
        frame->holdDurationTicks = hold;
        frame->transform = Transform{tx, ty, scale, rot, op};
        frame->easing = (Easing) easing;
        frame->layers.clear();

        for (uint32_t li = 0; li < layerCount; ++li) {
            uint32_t nameLen;
            if (!readRaw(in, nameLen)) return false;
            std::string name(nameLen, '\0');
            if (nameLen > 0) in.read(&name[0], (std::streamsize) nameLen);
            uint8_t visible, locked, blendMode;
            float opacity;
            uint32_t strokeCount;
            if (!in || !readRaw(in, visible) || !readRaw(in, locked) ||
                !readRaw(in, opacity) || !readRaw(in, blendMode) || !readRaw(in, strokeCount)) {
                return false;
            }

            auto layer = std::make_unique<Layer>(name);
            layer->visible = visible != 0;
            layer->locked = locked != 0;
            layer->opacity = opacity;
            layer->blendMode = (BlendMode) blendMode;

            for (uint32_t si = 0; si < strokeCount; ++si) {
                uint32_t colorArgb;
                Brush b{};
                uint8_t pas, pao, bm;
                uint32_t pointCount;
                if (!readRaw(in, colorArgb) ||
                    !readRaw(in, b.baseSizePx) || !readRaw(in, b.minSizeFactor) ||
                    !readRaw(in, b.opacity) || !readRaw(in, b.hardness) || !readRaw(in, b.spacing) ||
                    !readRaw(in, pas) || !readRaw(in, pao) || !readRaw(in, bm) ||
                    !readRaw(in, pointCount)) {
                    return false;
                }
                b.pressureAffectsSize = pas != 0;
                b.pressureAffectsOpacity = pao != 0;
                b.blendMode = (BlendMode) bm;

                auto stroke = std::make_unique<Stroke>((uint64_t) si + 1, b, colorArgb);
                for (uint32_t pi = 0; pi < pointCount; ++pi) {
                    DrawPoint p{};
                    if (!readRaw(in, p.x) || !readRaw(in, p.y) || !readRaw(in, p.pressure) ||
                        !readRaw(in, p.tiltX) || !readRaw(in, p.tiltY) || !readRaw(in, p.timestampNanos)) {
                        return false;
                    }
                    stroke->addPoint(p);
                }
                stroke->finish();
                layer->addStroke(std::move(stroke));
            }
            frame->layers.push_back(std::move(layer));
        }
        if (frame->layers.empty()) {
            frame->layers.push_back(std::make_unique<Layer>("Camada 1"));
        }
    }
    return true;
}

} // namespace dreams

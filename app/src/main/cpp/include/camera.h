#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include "engine_types.h"

namespace dreams {

// Pose da câmera em coordenadas de canvas (pixels). A câmera é um objeto de cena
// separado da vista de edição: mover a câmera não move o que você enxerga ao
// editar (ideia do Pencil2D 0.7 / OpenToonz).
//   centerX/centerY: ponto do canvas para onde a câmera aponta.
//   zoom: 1 = o quadro da câmera cobre o canvas inteiro; 2 = enxerga metade da área.
//   rotationDeg: rotação do quadro da câmera (o conteúdo gira no sentido oposto na vista).
struct CameraPose {
    float centerX = 0.f;
    float centerY = 0.f;
    float zoom = 1.f;
    float rotationDeg = 0.f;
};

struct CameraKey {
    int frameIndex = 0;
    CameraPose pose;
    // Easing do trecho que COMEÇA nesta chave (até a próxima chave).
    Easing easing = Easing::EaseInOut;
    // Hold (interpolação em degrau): mantém esta pose até a próxima chave, sem tween.
    // Existe porque em apps reais a câmera "sempre interpolar" incomoda (ver README).
    bool hold = false;
};

class CameraTrack {
public:
    const std::vector<CameraKey>& keys() const { return keys_; }
    size_t count() const { return keys_.size(); }

    CameraKey* keyAt(int frameIndex) {
        for (auto& k : keys_) if (k.frameIndex == frameIndex) return &k;
        return nullptr;
    }
    const CameraKey* keyAt(int frameIndex) const {
        for (const auto& k : keys_) if (k.frameIndex == frameIndex) return &k;
        return nullptr;
    }

    // Insere ou substitui a chave do frame, mantendo a lista ordenada por frame.
    CameraKey& setKey(const CameraKey& key) {
        if (CameraKey* existing = keyAt(key.frameIndex)) { *existing = key; return *existing; }
        keys_.push_back(key);
        std::sort(keys_.begin(), keys_.end(),
                  [](const CameraKey& a, const CameraKey& b) { return a.frameIndex < b.frameIndex; });
        return *keyAt(key.frameIndex);
    }

    bool removeKey(int frameIndex) {
        auto it = std::remove_if(keys_.begin(), keys_.end(),
                                 [&](const CameraKey& k) { return k.frameIndex == frameIndex; });
        bool removed = it != keys_.end();
        keys_.erase(it, keys_.end());
        return removed;
    }

    // Pose efetiva em um frame: sem chaves = defaultPose; antes da primeira / depois
    // da última = pose dessa chave; entre duas chaves = interpolação com o easing da
    // chave anterior (ou degrau, se hold). Zoom interpola de forma geométrica
    // (multiplicativa), que parece uniforme ao olho; posição e rotação são lineares.
    CameraPose resolve(int frameIndex, const CameraPose& defaultPose) const {
        if (keys_.empty()) return defaultPose;
        if (frameIndex <= keys_.front().frameIndex) return keys_.front().pose;
        if (frameIndex >= keys_.back().frameIndex) return keys_.back().pose;

        size_t next = 1;
        while (next < keys_.size() && keys_[next].frameIndex <= frameIndex) ++next;
        const CameraKey& a = keys_[next - 1];
        const CameraKey& b = keys_[next];
        if (a.hold) return a.pose;

        float span = (float) (b.frameIndex - a.frameIndex);
        float t = span > 0.f ? (float) (frameIndex - a.frameIndex) / span : 0.f;
        t = applyEasing(a.easing, t);

        CameraPose p;
        p.centerX = a.pose.centerX + (b.pose.centerX - a.pose.centerX) * t;
        p.centerY = a.pose.centerY + (b.pose.centerY - a.pose.centerY) * t;
        p.rotationDeg = a.pose.rotationDeg + (b.pose.rotationDeg - a.pose.rotationDeg) * t;
        float za = std::max(a.pose.zoom, 0.01f);
        float zb = std::max(b.pose.zoom, 0.01f);
        p.zoom = za * std::pow(zb / za, t);
        return p;
    }

private:
    std::vector<CameraKey> keys_;
};

} // namespace dreams

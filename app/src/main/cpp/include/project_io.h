#pragma once
#include <string>
#include "timeline.h"

namespace dreams {

// Serialização binária própria do projeto (frames, camadas, traços com todos os
// pontos e o pincel usado). Sem dependências externas. Assume little-endian e
// mesmo layout de struct entre gravação e leitura — válido para salvar/carregar
// no mesmo dispositivo/build, NÃO é um formato de troca entre plataformas.
bool saveTimelineToFile(const Timeline& timeline, const std::string& path);
bool loadTimelineFromFile(Timeline& timeline, const std::string& path);

} // namespace dreams

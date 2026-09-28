# Traço Dreams (DreamMotion)

Versão Android inspirada no Procreate Dreams: desenho em camadas + timeline de animação
com keyframes, agora com **câmera virtual**. Este repositório é um esqueleto funcional em
evolução, não o app completo.

## Arquitetura

- **Engine em C++** (`app/src/main/cpp`): estado de desenho (traços, camadas, frames,
  timeline, pincel, undo/redo, câmera) e renderização OpenGL ES 3 em thread nativa
  dedicada com contexto EGL próprio.
- **Ponte JNI** (`native_bridge.cpp` + `NativeEngine.kt`).
- **UI Android** (Views programáticas): `DreamsSurfaceView` (canvas), `BrushPanel`,
  `LayersPanel` (camadas/timeline/playback/keyframes de objeto) e `CameraPanel`.

## Pipeline de renderização

1. Cada camada suja é re-rasterizada no seu FBO.
2. Todas as camadas são compostas numa **textura de cena** (tamanho do canvas).
3. A cena é apresentada na tela de duas formas:
   - **Vista de edição**: canvas inteiro + overlay da câmera (moldura, cruz de centro,
     marca de topo, caminho entre chaves).
   - **Vista da câmera**: só o que o quadro da câmera enxerga, com letterbox
     (scissor); desenho desativado, pois os toques não estão em coordenadas de canvas.

## Câmera — o que foi pesquisado e como virou design

Referências estudadas: Pencil2D 0.7, OpenToonz e Procreate Dreams.

| Ideia | Origem | Nossa implementação |
|---|---|---|
| Câmera é um objeto de cena, não a vista de edição | Pencil2D 0.7 (sistema "object-based") | `CameraTrack` separado; vista de edição não é afetada |
| Reset individual (posição/zoom/rotação) ou tudo | Pencil2D 0.7 | 4 botões Reset |
| Caminho da câmera visível no canvas | Pencil2D 0.7 ("Show path") | Overlay com polilinha + marcadores nas chaves |
| Easing na interpolação da câmera | Pencil2D 0.7, Procreate Dreams | Easing por chave (linear/in/out/in-out) |
| Toda transformação define uma chave | OpenToonz | Mover/zoom/girar cria chave no frame atual |
| Quadro da câmera com proporção escolhável | OpenToonz (resoluções predefinidas) | Presets Canvas, 16:9, 4:3, 1:1, 9:16 |
| Hold (sem tween) | Pedido recorrente de usuários do Pencil2D | Flag `hold` por chave |
| Separar vista de trabalho da câmera | Issue de usuários do Pencil2D (não dava para dar zoom-out sem keyframar) | Vista de edição e vista da câmera são modos distintos |

Zoom é interpolado de forma geométrica (multiplicativa) para parecer uniforme; posição e
rotação são lineares. Antes da primeira chave e depois da última a pose é mantida.

### Ainda não feito na câmera

- Zoom/pan da **vista de edição** (hoje o canvas é sempre mostrado inteiro).
- Manipulação direta por gesto (arrastar a moldura); hoje são botões de nudge.
- "Performing" estilo Procreate Dreams (gravar o gesto e virar chaves, com suavização).
- Várias câmeras (OpenToonz) e câmera com profundidade/parallax por camada.
- Onion skin da câmera; marcadores de chave da câmera numa trilha da timeline.
- Undo/redo de operações de câmera.
- A vista da câmera amostra a textura do canvas (com zoom > 1 fica suave/borrada); a
  exportação precisará re-renderizar na resolução de saída.

## O que já funciona (resumo)

Desenho com pincel (cor/tamanho/dureza/borracha), undo/redo de traços, camadas com blend
modes, frames e playback, keyframes de objeto com interpolação real, e a câmera acima.

## Próximos passos

- **Salvar/carregar projeto** (hoje fechar o app perde tudo) — próximo item.
- Exportação, onion skinning, seletor de cor livre, opacidade do pincel na UI,
  cross-fade entre keyframes, undo estrutural, estilo visual dos painéis.

## Concorrência

Toda estrutura compartilhada (`Timeline`, índices, pincel, undo/redo, câmera) é protegida
por `timelineMutex_`; `playing_` é `std::atomic<bool>`.

## Build

- Android Studio Koala+ com NDK 26.3.11579264 e CMake 3.22.1.
- `minSdk 26`, `compileSdk`/`targetSdk 34`.

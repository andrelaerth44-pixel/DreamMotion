# Traço Dreams (DreamMotion)

Versão Android inspirada no Procreate Dreams: desenho em camadas + timeline de animação
com keyframes interpolados. Este commit é um **esqueleto funcional em evolução**, não
o app completo — mas já compila, desenha, composita camadas e navega frames.

## Arquitetura

- **Engine em C++** (`app/src/main/cpp`): dona de todo o estado de desenho (traços, camadas,
  frames, timeline) e da renderização OpenGL ES 3. Roda em uma **thread de render nativa
  dedicada**, com seu próprio contexto EGL — não usa `GLSurfaceView`.
- **Ponte JNI** (`native_bridge.cpp` + `NativeEngine.kt`): toque, ciclo de vida da
  `Surface`, e agora também consultas/comandos de estrutura (camadas e frames).
- **UI Android**: `DreamsSurfaceView` (canvas cru) + `LayersPanel` (painel utilitário
  de camadas/timeline, Views programáticas sem estilo ainda).

## O que já funciona

- Compila um app Android com módulo nativo (CMake + NDK).
- Desenho com pincel: carimbo circular com borda suave, reagindo à pressão.
- **Compositing por camada via FBO**, com blend modes reais (Normal/Multiply/Screen/Add)
  e borracha como blend mode próprio (reduz alpha em vez de pintar).
- **VBO persistente** para geometria de traço: um upload + um draw call por traço
  inteiro, em vez de um VBO por ponto.
- **Painel de camadas**: adicionar, remover, reordenar, mostrar/ocultar, escolher a
  camada ativa (onde novos traços vão parar).
- **Navegação de frames**: adicionar frame, ir para o anterior/próximo — cada frame
  tem seu próprio conjunto de camadas.
- Estrutura de dados de `Timeline`/`Frame` já suporta keyframes e frames interpolados,
  com `resolveTransformAtTick` fazendo lerp de `Transform` entre keyframes vizinhos
  (ainda não exposto na UI nem usado no playback).

## O que ainda é esqueleto / próximos passos

- **Playback da timeline** (reproduzir os frames em sequência na framerate do projeto)
  — hoje a navegação é manual, quadro a quadro.
- **Fração real de interpolação**: `Timeline::resolveTransformAtTick` usa `t = 0.5f`
  fixo como placeholder.
- **Onion skinning**, seletor de pincel/cor, undo/redo, exportação (vídeo/GIF/PNG
  sequence), formato de arquivo de projeto (salvar/carregar).
- **Estilo visual do painel de camadas**: hoje é puramente funcional (Views cruas).
- **Multi-toque / zoom e pan do canvas**: não implementados ainda.

## Concorrência

A engine agora é acessada por duas threads: a de render (dona do contexto EGL) e a
UI thread (via os métodos de camadas/frames chamados pelo `LayersPanel`). Toda
estrutura compartilhada (`Timeline`, `currentFrameIndex_`, `activeLayerIndex_`) está
protegida por um `std::mutex` (`timelineMutex_`) — ver comentários em
`gl_render_engine.h`.

## Build

- Android Studio Koala+ com NDK 26.3.11579264 e CMake 3.22.1 instalados via SDK Manager.
- `minSdk 26` (OpenGL ES 3 garantido), `compileSdk`/`targetSdk 34`.
- Abrir a raiz do repo no Android Studio e rodar — o Gradle já aponta para
  `app/src/main/cpp/CMakeLists.txt` via `externalNativeBuild`.

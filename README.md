# Traço Dreams (DreamMotion)

Versão Android inspirada no Procreate Dreams: desenho em camadas + timeline de animação
com keyframes interpolados. Este commit é um **esqueleto funcional em evolução**, não
o app completo — mas já compila, desenha, composita camadas, navega frames e reproduz
a timeline.

## Arquitetura

- **Engine em C++** (`app/src/main/cpp`): dona de todo o estado de desenho (traços, camadas,
  frames, timeline) e da renderização OpenGL ES 3. Roda em uma **thread de render nativa
  dedicada**, com seu próprio contexto EGL — não usa `GLSurfaceView`. Essa mesma thread
  agora também cuida do playback (avançar frames automaticamente).
- **Ponte JNI** (`native_bridge.cpp` + `NativeEngine.kt`): toque, ciclo de vida da
  `Surface`, consultas/comandos de estrutura (camadas e frames) e controle de playback.
- **UI Android**: `DreamsSurfaceView` (canvas cru) + `LayersPanel` (painel utilitário
  de camadas/timeline/playback, Views programáticas sem estilo ainda).

## O que já funciona

- Compila um app Android com módulo nativo (CMake + NDK).
- Desenho com pincel: carimbo circular com borda suave, reagindo à pressão.
- Compositing por camada via FBO, com blend modes reais (Normal/Multiply/Screen/Add)
  e borracha como blend mode próprio.
- VBO persistente para geometria de traço (um upload + um draw call por traço inteiro).
- Painel de camadas: adicionar, remover, reordenar, mostrar/ocultar, escolher a
  camada ativa.
- Navegação de frames: adicionar frame, ir para o anterior/próximo — cada frame
  tem seu próprio conjunto de camadas.
- **Playback**: botão Play/Pause reproduz a timeline em loop, respeitando o
  `holdDurationTicks` de cada frame e o framerate do projeto (24fps por padrão) —
  quem avança os frames é a própria render thread nativa (`advancePlayback`),
  não um timer do lado Kotlin.
- Estrutura de dados de `Timeline`/`Frame` já suporta keyframes e frames interpolados,
  com `resolveTransformAtTick` fazendo lerp de `Transform` (ainda não usado no playback
  visual — hoje o playback troca de frame inteiro, não interpola transforms).

## O que ainda é esqueleto / próximos passos

- **Interpolação real no playback**: hoje `FrameType::Keyframe`/`Interpolated` existem
  na estrutura de dados mas o playback atual só alterna entre frames `Drawn` inteiros;
  falta ligar `resolveTransformAtTick` ao desenho de fato.
- **Desenhar durante o playback não é bloqueado** — um toque no canvas enquanto toca
  adiciona um traço ao frame que estiver passando naquele instante. Bloquear input
  durante o play (ou pausar automaticamente ao tocar) é um próximo passo óbvio.
- **Fração real de interpolação**: `Timeline::resolveTransformAtTick` usa `t = 0.5f`
  fixo como placeholder.
- **Onion skinning**, seletor de pincel/cor, undo/redo, exportação (vídeo/GIF/PNG
  sequence), formato de arquivo de projeto (salvar/carregar).
- **Estilo visual do painel**: hoje é puramente funcional (Views cruas).
- **Multi-toque / zoom e pan do canvas**: não implementados ainda.

## Concorrência

A engine é acessada por duas threads: a de render (dona do contexto EGL, do
playback e do desenho) e a UI thread (via os métodos de camadas/frames/playback
chamados pelo `LayersPanel`). Toda estrutura compartilhada (`Timeline`,
`currentFrameIndex_`, `activeLayerIndex_`) está protegida por um `std::mutex`
(`timelineMutex_`); o flag `playing_` é um `std::atomic<bool>` por ser um caso
mais simples (leitura/escrita de um booleano isolado).

## Build

- Android Studio Koala+ com NDK 26.3.11579264 e CMake 3.22.1 instalados via SDK Manager.
- `minSdk 26` (OpenGL ES 3 garantido), `compileSdk`/`targetSdk 34`.
- Abrir a raiz do repo no Android Studio e rodar — o Gradle já aponta para
  `app/src/main/cpp/CMakeLists.txt` via `externalNativeBuild`.

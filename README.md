# Traço Dreams (DreamMotion)

Versão Android inspirada no Procreate Dreams: desenho em camadas + timeline de animação
com keyframes interpolados. Este commit é um **esqueleto funcional**, não o app completo —
um ponto de partida real sobre o qual dá para iterar.

## Arquitetura

- **Engine em C++** (`app/src/main/cpp`): dona de todo o estado de desenho (traços, camadas,
  frames, timeline) e da renderização OpenGL ES 3, para tirar o trabalho pesado da JVM/UI
  thread. Roda em uma **thread de render nativa dedicada**, com seu próprio contexto EGL —
  não usa `GLSurfaceView`, que enfileira tudo na thread Java.
- **Ponte JNI** (`native_bridge.cpp` + `NativeEngine.kt`): superfície mínima — ciclo de vida
  da `Surface` e eventos de toque (posição + pressão) cruzam para o C++; o resto (estado de
  traços, camadas, blending, GL) fica inteiramente do lado nativo.
- **UI Android** (`DreamsSurfaceView` + `MainActivity`): hoje é só um `SurfaceView` cru
  repassando eventos. Painéis de pincel/camadas/timeline ainda não existem.

## O que já funciona

- Compila um app Android com módulo nativo (CMake + NDK).
- Ao tocar na tela, um traço é criado e enviado à engine nativa com pressão por ponto.
- A engine renderiza cada ponto do traço como um carimbo circular (borda suave via
  `smoothstep`, controlada por `hardness`), com tamanho modulado pela pressão.
- Estrutura de dados de `Timeline`/`Frame` já suporta frames desenhados, keyframes e
  frames interpolados, com `resolveTransformAtTick` fazendo lerp de `Transform` entre
  keyframes vizinhos.

## O que ainda é esqueleto / próximos passos

- **Compositing por camada via FBO**: hoje todo traço é desenhado direto no framebuffer
  padrão. O próximo passo natural é `renderStrokeToLayer` (textura+FBO por `Layer`) e
  `compositeLayer` (blend modes reais: multiply/screen/add/erase).
- **Performance do stroke rendering**: a versão atual cria/destrói um VBO por ponto por
  frame (`glGenBuffers`/`glDeleteBuffers` dentro do loop) — correto, mas longe do ideal.
  Trocar por um VBO persistente com buffer circular ou instancing é o ganho de performance
  mais óbvio antes de qualquer teste em dispositivo real.
- **Fração real de interpolação**: `Timeline::resolveTransformAtTick` usa `t = 0.5f` fixo
  como placeholder — falta calcular a fração real com base nos ticks acumulados entre o
  keyframe anterior e o próximo.
- **UI**: seletor de pincel, painel de camadas, trilha de timeline com scrubber, undo/redo,
  exportação (vídeo/GIF/PNG sequence), formato de arquivo de projeto (salvar/carregar).
- **Multi-toque / paleta de cores / zoom e pan do canvas**: não implementados ainda.

## Build

- Android Studio Koala+ com NDK 26.3.11579264 e CMake 3.22.1 instalados via SDK Manager.
- `minSdk 26` (OpenGL ES 3 garantido), `compileSdk`/`targetSdk 34`.
- Abrir a raiz do repo no Android Studio e rodar — o Gradle já aponta para
  `app/src/main/cpp/CMakeLists.txt` via `externalNativeBuild`.

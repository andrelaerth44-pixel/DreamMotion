# Traço Dreams (DreamMotion)

Versão Android inspirada no Procreate Dreams: desenho em camadas + timeline de animação
com keyframes interpolados de verdade. Este commit é um **esqueleto funcional em
evolução**, não o app completo — mas já compila, desenha, composita camadas, navega
frames, reproduz a timeline e interpola transform entre keyframes.

## Arquitetura

- **Engine em C++** (`app/src/main/cpp`): dona de todo o estado de desenho (traços,
  camadas, frames, timeline) e da renderização OpenGL ES 3. Roda em uma thread de
  render nativa dedicada, com seu próprio contexto EGL.
- **Ponte JNI** (`native_bridge.cpp` + `NativeEngine.kt`): toque, ciclo de vida da
  `Surface`, estrutura (camadas/frames), autoria de keyframes e playback.
- **UI Android**: `DreamsSurfaceView` (canvas cru) + `LayersPanel` (painel utilitário,
  Views programáticas sem estilo ainda).

## O que já funciona

- Compila um app Android com módulo nativo (CMake + NDK).
- Desenho com pincel: carimbo circular com borda suave, reagindo à pressão.
- Compositing por camada via FBO, com blend modes reais e VBO persistente de traço.
- Painel de camadas (add/remover/reordenar/visibilidade) e navegação de frames.
- Playback em loop, respeitando `holdDurationTicks`/framerate, avançado pela
  própria render thread nativa.
- **Interpolação de keyframes de verdade**: marque um frame como Keyframe, crie
  um frame Interpolated entre dois keyframes, ajuste a pose (posição/escala/
  rotação/opacidade) de cada keyframe com os botões de nudge do painel — no
  playback, o frame interpolado mostra o desenho do keyframe anterior sendo
  transformado (via shader) em direção à pose do próximo keyframe, com a fração
  real calculada pela posição entre os dois índices de keyframe (não mais um
  placeholder fixo).

### Limitação honesta da interpolação atual

Não existe (ainda) o conceito de "uma única arte compartilhada entre keyframes"
— cada Frame tem suas próprias camadas. Para um frame Interpolated, o conteúdo
vem do keyframe/frame desenhado **anterior mais próximo** (`Timeline::content-
SourceFrame`), transformado. Isso já produz o efeito visual certo (o desenho se
move/escala/gira), mas se você desenhar algo diferente no segundo keyframe, o
playback interpolado não faz cross-fade entre os dois desenhos — ele só
transforma o primeiro. Resolver isso de verdade pede um objeto de arte
compartilhado entre keyframes (ou um cross-fade explícito), documentado aqui
como próximo passo.

## O que ainda é esqueleto / próximos passos

- Cross-fade real entre a arte de dois keyframes diferentes (ver limitação acima).
- Desenhar durante o playback não é bloqueado.
- Onion skinning, seletor de pincel/cor, undo/redo, exportação, salvar/carregar projeto.
- Estilo visual do painel (hoje Views cruas).
- Multi-toque / zoom e pan do canvas.

## Concorrência

Toda estrutura compartilhada (`Timeline`, `currentFrameIndex_`, `activeLayerIndex_`)
está protegida por `timelineMutex_`; `playing_` é um `std::atomic<bool>`.

## Build

- Android Studio Koala+ com NDK 26.3.11579264 e CMake 3.22.1.
- `minSdk 26`, `compileSdk`/`targetSdk 34`.
- Abrir a raiz do repo no Android Studio e rodar — o Gradle aponta para
  `app/src/main/cpp/CMakeLists.txt` via `externalNativeBuild`.

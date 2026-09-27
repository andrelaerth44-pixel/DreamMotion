# Traço Dreams (DreamMotion)

Versão Android inspirada no Procreate Dreams: desenho em camadas + timeline de animação
com keyframes interpolados de verdade. Este commit é um **esqueleto funcional em
evolução**, não o app completo — mas já compila, desenha (com cor/tamanho/dureza/
borracha controláveis), composita camadas, navega frames, reproduz a timeline e
interpola transform entre keyframes.

## Arquitetura

- **Engine em C++** (`app/src/main/cpp`): dona de todo o estado de desenho (traços,
  camadas, frames, timeline, pincel atual) e da renderização OpenGL ES 3. Roda em
  uma thread de render nativa dedicada, com seu próprio contexto EGL.
- **Ponte JNI** (`native_bridge.cpp` + `NativeEngine.kt`): toque, ciclo de vida da
  `Surface`, controle de pincel, estrutura (camadas/frames), autoria de keyframes
  e playback.
- **UI Android**: `DreamsSurfaceView` (canvas cru) + `BrushPanel` (cor/tamanho/
  dureza/borracha) + `LayersPanel` (camadas/timeline/playback/keyframes). Todas
  Views programáticas, sem estilo visual ainda.

## O que já funciona

- Compila um app Android com módulo nativo (CMake + NDK).
- Desenho com pincel real: cor (paleta fixa de 7 cores), tamanho e dureza
  ajustáveis por SeekBar, borracha como blend mode próprio.
- Compositing por camada via FBO, blend modes reais, VBO persistente de traço.
- Painel de camadas (add/remover/reordenar/visibilidade) e navegação de frames.
- Playback em loop respeitando `holdDurationTicks`/framerate.
- Interpolação real de keyframes (posição/escala/rotação/opacidade), com fração
  calculada pela distância real entre índices de keyframe.

### Limitação honesta da interpolação

Não existe (ainda) uma "arte compartilhada entre keyframes" — um frame
Interpolated reaproveita o desenho do keyframe anterior mais próximo, transformado.
Se os dois keyframes tiverem desenhos diferentes, o interpolado não faz cross-fade
entre eles, só transforma o primeiro.

## O que ainda é esqueleto / próximos passos

- Cross-fade real entre a arte de dois keyframes diferentes.
- Seletor de cor livre (roda HSV) em vez de paleta fixa; controle de opacidade
  do pincel (o campo já existe no `Brush`, só falta UI).
- Desenhar durante o playback não é bloqueado.
- Onion skinning, undo/redo, exportação, salvar/carregar projeto.
- Estilo visual dos painéis (hoje Views cruas).
- Multi-toque / zoom e pan do canvas.

## Concorrência

Toda estrutura compartilhada (`Timeline`, índices correntes, pincel atual) está
protegida por `timelineMutex_`; `playing_` é um `std::atomic<bool>`.

## Build

- Android Studio Koala+ com NDK 26.3.11579264 e CMake 3.22.1.
- `minSdk 26`, `compileSdk`/`targetSdk 34`.
- Abrir a raiz do repo no Android Studio e rodar — o Gradle aponta para
  `app/src/main/cpp/CMakeLists.txt` via `externalNativeBuild`.

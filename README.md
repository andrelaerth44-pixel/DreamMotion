# Traço Dreams (DreamMotion)

Versão Android inspirada no Procreate Dreams: desenho em camadas + timeline de animação
com keyframes interpolados de verdade. Este commit é um **esqueleto funcional em
evolução**, não o app completo — mas já compila, desenha (com cor/tamanho/dureza/
borracha controláveis, undo/redo), composita camadas, navega frames, reproduz a
timeline e interpola transform entre keyframes.

## Arquitetura

- **Engine em C++** (`app/src/main/cpp`): dona de todo o estado de desenho (traços,
  camadas, frames, timeline, pincel atual, histórico de undo/redo) e da
  renderização OpenGL ES 3. Roda em uma thread de render nativa dedicada.
- **Ponte JNI** (`native_bridge.cpp` + `NativeEngine.kt`): toque, ciclo de vida
  da `Surface`, pincel, undo/redo, estrutura (camadas/frames), keyframes e playback.
- **UI Android**: `DreamsSurfaceView` (canvas cru) + `BrushPanel` (cor/tamanho/
  dureza/borracha/undo/redo) + `LayersPanel` (camadas/timeline/playback/keyframes).

## O que já funciona

- Compila um app Android com módulo nativo (CMake + NDK).
- Desenho com pincel real: cor, tamanho, dureza, borracha.
- Compositing por camada via FBO, blend modes reais, VBO persistente de traço.
- **Undo/redo em nível de traço completo**: cada traço terminado vira uma
  entrada na pilha de undo; desfazer remove o último traço da camada onde ele
  foi desenhado (mesmo que você tenha trocado de camada/frame depois); começar
  um traço novo limpa a pilha de redo, como em qualquer editor.
- Painel de camadas e navegação de frames.
- Playback em loop respeitando `holdDurationTicks`/framerate.
- Interpolação real de keyframes (posição/escala/rotação/opacidade).

### Limitações honestas

- Undo/redo só cobre traços — adicionar/remover camada ou frame não entra na
  pilha (desfazer não traz uma camada removida de volta, por exemplo).
- Interpolação de keyframes não tem "arte compartilhada": um frame Interpolated
  reaproveita o desenho do keyframe anterior mais próximo, transformado; não faz
  cross-fade entre desenhos diferentes em dois keyframes.

## O que ainda é esqueleto / próximos passos

- Undo/redo também para operações estruturais (camada, frame).
- Cross-fade real entre a arte de dois keyframes diferentes.
- Seletor de cor livre (HSV); opacidade do pincel na UI.
- Desenhar durante o playback não é bloqueado.
- Onion skinning, exportação, salvar/carregar projeto.
- Estilo visual dos painéis (hoje Views cruas).
- Multi-toque / zoom e pan do canvas.

## Concorrência

Toda estrutura compartilhada (`Timeline`, índices correntes, pincel atual,
pilhas de undo/redo) está protegida por `timelineMutex_`; `playing_` é um
`std::atomic<bool>`.

## Build

- Android Studio Koala+ com NDK 26.3.11579264 e CMake 3.22.1.
- `minSdk 26`, `compileSdk`/`targetSdk 34`.
- Abrir a raiz do repo no Android Studio e rodar — o Gradle aponta para
  `app/src/main/cpp/CMakeLists.txt` via `externalNativeBuild`.

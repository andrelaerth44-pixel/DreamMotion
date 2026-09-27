# Traço Dreams (DreamMotion)

Versão Android inspirada no Procreate Dreams: desenho em camadas + timeline de animação
com keyframes interpolados de verdade, câmera de navegação do canvas e salvar/carregar
projeto. Este commit é um **esqueleto funcional em evolução**, não o app completo.

## Arquitetura

- **Engine em C++** (`app/src/main/cpp`): dona de todo o estado (traços, camadas,
  frames, timeline, pincel, undo/redo, câmera de navegação) e da renderização
  OpenGL ES 3, em uma thread de render nativa dedicada.
- **Ponte JNI** (`native_bridge.cpp` + `NativeEngine.kt`): toque, ciclo de vida
  da `Surface`, câmera, pincel, undo/redo, estrutura, keyframes, playback e
  agora salvar/carregar projeto.
- **UI Android**: `DreamsSurfaceView` (canvas + gestos de 1 e 2 dedos) +
  `BrushPanel` + `LayersPanel`.
- **Persistência** (`project_io.h/.cpp`): serialização binária própria de toda
  a `Timeline` (frames, camadas, traços com todos os pontos e o pincel usado).

## O que já funciona

- Desenho com pincel real (cor/tamanho/dureza/borracha), compositing por FBO
  com blend modes, VBO persistente, undo/redo em nível de traço.
- Camadas, navegação de frames, playback em loop, interpolação real de
  keyframes.
- **Câmera de navegação do canvas**: gesto de dois dedos faz pan + pinch-zoom +
  rotação do canvas, independente do Transform de animação dos keyframes —
  composta no mesmo shader como um segundo estágio. Um dedo continua
  desenhando normalmente; toques de desenho são convertidos de coordenada de
  tela para coordenada de canvas levando a câmera em conta, então o traço cai
  no lugar certo mesmo com zoom/pan/rotação ativos. Botão "⌖ Reset Câmera"
  no painel.
- **Salvar/carregar projeto**: botões "💾 Salvar"/"📂 Carregar" no painel,
  gravando em `context.filesDir/project.tdrm`.

### Limitações honestas

- Salvar/carregar é **manual** — sem autosave, sem carregamento automático ao
  abrir o app. Fechar o app sem apertar "Salvar" ainda perde o trabalho.
- O formato de arquivo (`TDRM`) é binário próprio, assume little-endian e o
  mesmo layout de struct entre gravação e leitura — válido só para o mesmo
  dispositivo/build, não é um formato de troca entre plataformas ou versões
  futuras incompatíveis do app.
- Ao carregar um projeto, os handles de textura/FBO OpenGL das camadas
  antigas (de antes do load) não são liberados explicitamente
  (`glDeleteTextures`/`glDeleteFramebuffers`) — um vazamento de recursos GL
  conhecido, aceitável numa sessão de uso normal mas que idealmente seria
  corrigido com um destrutor de `Layer` ciente do contexto GL.
- O ajuste de pivot no pinch-zoom (`zoomCamera`) não leva a rotação da câmera
  em conta — zoom+rotação simultâneos podem fazer o pivot deslizar um pouco.
- Undo/redo continua cobrindo só traços, não operações estruturais.
- Interpolação de keyframes ainda não tem "arte compartilhada"/cross-fade
  entre desenhos diferentes em dois keyframes.

## O que ainda é esqueleto / próximos passos

- Autosave / carregamento automático ao abrir o app.
- Cross-fade real entre keyframes com desenhos diferentes.
- Seletor de cor livre (HSV); opacidade do pincel na UI.
- Onion skinning, exportação (vídeo/GIF/PNG sequence).
- Estilo visual dos painéis (hoje Views cruas).

## Concorrência

Toda estrutura compartilhada (`Timeline`, índices correntes, pincel, câmera,
pilhas de undo/redo) está protegida por `timelineMutex_`; `playing_` é um
`std::atomic<bool>`.

## Build

- Android Studio Koala+ com NDK 26.3.11579264 e CMake 3.22.1.
- `minSdk 26`, `compileSdk`/`targetSdk 34`.
- Abrir a raiz do repo no Android Studio e rodar — o Gradle aponta para
  `app/src/main/cpp/CMakeLists.txt` via `externalNativeBuild`.

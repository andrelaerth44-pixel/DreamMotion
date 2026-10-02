# Traço Dreams (DreamMotion)

Versão Android inspirada no Procreate Dreams: desenho em camadas + timeline de animação
com keyframes, câmera virtual e projeto salvável. Esqueleto funcional em evolução,
não o app completo. **Nada aqui foi compilado nem executado em dispositivo ainda** —
análise e testes pontuais foram feitos só para a lógica de JSON (fora do Android).

## Arquitetura

- **Engine em C++** (`app/src/main/cpp`): estado de desenho (traços, camadas, frames,
  timeline, pincel, undo/redo, câmera, salvar/carregar) e renderização OpenGL ES 3
  em thread nativa dedicada com contexto EGL próprio.
- **Ponte JNI** (`native_bridge.cpp` + `NativeEngine.kt`).
- **UI Android** (Views programáticas): `DreamsSurfaceView`, `ProjectPanel`
  (salvar/carregar), `BrushPanel`, `LayersPanel`, `CameraPanel`.

## Salvar / carregar projeto

- Formato: JSON próprio (`include/json.h` + `src/json.cpp`), sem dependência externa.
  Parser/writer testado por round-trip (inclusive cores ARGB com bit de sinal) fora
  do repositório.
- Salvo: framerate, frame atual, frames (tipo, duração, transform, easing) com camadas
  (nome, visibilidade, opacidade, blend) e traços (cor, pincel, pontos com pressão),
  trilha de câmera inteira e pincel atual.
- **Autosave** em `MainActivity.onPause()`; **autoload** em
  `DreamsSurfaceView.surfaceCreated` (no-op silencioso se o arquivo não existir).
- Botões manuais Salvar/Carregar no `ProjectPanel`. Um único slot
  (`files/current_project.json`) — sem gerenciador de múltiplos projetos ainda.

## Gestão de recursos GL (corrigido)

Antes, remover uma camada ou carregar um projeto descartava objetos `Layer` sem
liberar as texturas/FBOs OpenGL deles (vazamento na GPU). Agora:

- `removeLayer` e `loadProjectFromFile` (que roda na UI thread, sem contexto EGL)
  **enfileiram** os handles (`queueLayerGLResourcesForDeletion`); no load, enfileira
  as camadas de **todos** os frames, não só do corrente.
- A render thread esvazia a fila no início de cada `drawFrame`
  (`drainPendingGLDeletions`), onde o contexto está corrente.
- No resize (que já roda na render thread), os handles antigos são deletados direto —
  e agora de **todos** os frames; antes só o frame corrente era invalidado, deixando
  camadas de outros frames com textura do tamanho antigo.

Ainda não coberto: se o contexto EGL for destruído e recriado (ex.: Surface
recriada ao voltar do background), os handles guardados nas camadas passam a ser
inválidos no novo contexto e nada os re-aloca — hoje o `surfaceDestroyed` para a
engine (`stop`) mas o objeto continua vivo com handles obsoletos. Precisa ser
tratado junto com o ciclo de vida da Surface.

## Pipeline de renderização

1. Cada camada suja é re-rasterizada no seu FBO.
2. Todas as camadas são compostas numa textura de cena (tamanho do canvas).
3. A cena é apresentada: vista de edição (canvas inteiro + overlay da câmera) ou
   vista da câmera (letterbox, desenho desativado).

## Câmera (resumo)

Objeto de cena com chaves por frame (posição/zoom/rotação), easing e hold por
chave, presets de proporção, caminho visível no overlay — inspirado em Pencil2D
0.7, OpenToonz e Procreate Dreams.

## Próximos passos

- **Ciclo de vida da Surface**: ao voltar do background a Surface é recriada; hoje
  `surfaceCreated` chama `start()` de novo mas `stop()` já encerrou a thread e
  destruiu o contexto, deixando handles GL obsoletos nas camadas (ver acima). É o
  risco mais provável de bug visível em uso real.
- Gerenciador de múltiplos projetos/arquivos.
- Exportação, onion skinning, seletor de cor livre, opacidade do pincel na UI,
  cross-fade entre keyframes, undo estrutural, estilo visual dos painéis.

## Concorrência

Toda estrutura compartilhada (`Timeline`, índices, pincel, undo/redo, câmera, fila de
deleção GL) é protegida por `timelineMutex_`; `playing_` é `std::atomic<bool>`.

## Build

- Android Studio Koala+ com NDK 26.3.11579264 e CMake 3.22.1.
- `minSdk 26`, `compileSdk`/`targetSdk 34`.

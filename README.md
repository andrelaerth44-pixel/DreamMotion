# Traço Dreams (DreamMotion)

Versão Android inspirada no Procreate Dreams: desenho em camadas + timeline de animação
com keyframes, câmera virtual e projeto salvável. Esqueleto funcional em evolução,
não o app completo.

## O que foi (e não foi) verificado

**Nunca rodou em dispositivo nem foi compilado com o NDK/Gradle.** Verificações feitas
em sandbox Linux comum (g++ 13, C++17):

- `json.h/json.cpp`: teste de round-trip (gerar → parsear → comparar), incluindo
  cor ARGB com bit de sinal.
- `gl_render_engine.cpp` (+ `timeline`, `stroke`, `json`): **compila sem erros** com
  `-Wall -Wextra` contra *stubs* de EGL/GLES/Android que declaram só as funções
  usadas, e uma checagem de símbolos (`nm`) confirmou que todo método `dreams::`
  declarado/usado tem definição. Isso pega erros de tipo, includes e métodos
  declarados sem corpo — **não** pega erros de uso da API GL real (ordem de chamadas,
  estados), shaders (só o driver compila GLSL), nem JNI/Kotlin/Gradle/CMake.
- Os avisos restantes (`-Wmissing-field-initializers` em `RenderCommand`) são
  inofensivos.

## Arquitetura

- **Engine em C++** (`app/src/main/cpp`): estado de desenho (traços, camadas, frames,
  timeline, pincel, undo/redo, câmera, salvar/carregar) e renderização OpenGL ES 3
  em thread nativa dedicada com contexto EGL próprio.
- **Ponte JNI** (`native_bridge.cpp` + `NativeEngine.kt`).
- **UI Android** (Views programáticas): `DreamsSurfaceView`, `ProjectPanel`,
  `BrushPanel`, `LayersPanel`, `CameraPanel`.

## Ciclo de vida da Surface (corrigido nesta revisão)

Ao ir para background a `Surface` é destruída (`stop()`); ao voltar é criada outra
(`start()`), com um **contexto EGL novo**. Antes disso, os handles de textura/FBO
das camadas, da cena e o tamanho do VBO de traços continuavam marcados como
"já alocados" e o app renderizaria com ids inválidos (tela em branco / erros GL).
Agora:

- `resetGLStateForNewContext()` roda na render thread logo após `initEGL`: esquece
  handles de **todas** as camadas de todos os frames, da cena e do VBO, limpa as
  filas de deleção (ids antigos poderiam coincidir com objetos novos) e fecha um
  traço interrompido pela perda da Surface, registrando-o no undo.
- `start()` descarta comandos de uma sessão anterior (um `Shutdown` não consumido
  mataria a thread nova logo ao iniciar) e faz `join` de uma thread que morreu
  sozinha (atribuir uma `std::thread` joinable chama `std::terminate`).
- `stop()` sempre faz `join` se houver thread, mesmo que ela já tenha encerrado por
  falha de EGL (destruir uma `std::thread` joinable também chama `terminate`).
- `destroyEGL()` libera a referência do `ANativeWindow` mesmo quando `initEGL`
  falhou cedo (antes vazava), e `start()` devolve a referência extra se já estava
  rodando.

## Salvar / carregar projeto

- JSON próprio (`include/json.h` + `src/json.cpp`), sem dependência externa.
- Salvo: framerate, frame atual, frames (tipo, duração, transform, easing) com
  camadas e traços (cor, pincel, pontos com pressão), trilha de câmera e pincel atual.
- **Autosave** em `MainActivity.onPause()`; **autoload** em
  `DreamsSurfaceView.surfaceCreated`. Botões manuais no `ProjectPanel`. Um único
  slot (`files/current_project.json`).

### Ponto de atenção conhecido

`surfaceCreated` chama `nativeLoadProject` toda vez — inclusive ao **voltar do
background**. Como o `onPause` acabou de salvar o estado, o load recarrega o mesmo
conteúdo (inofensivo, mas zera o histórico de undo/redo e cancela qualquer estado
não salvo desde então). Idealmente o autoload só aconteceria na primeira criação.

## Gestão de recursos GL

`removeLayer` e `loadProjectFromFile` (UI thread, sem contexto) enfileiram handles
para a render thread deletar em `drawFrame`; o resize (que já roda na render thread)
deleta direto, em todos os frames.

## Pipeline de renderização

1. Cada camada suja é re-rasterizada no seu FBO.
2. Todas as camadas são compostas numa textura de cena (tamanho do canvas).
3. A cena é apresentada: vista de edição (canvas inteiro + overlay da câmera) ou
   vista da câmera (letterbox, desenho desativado).

## Câmera (resumo)

Objeto de cena com chaves por frame (posição/zoom/rotação), easing e hold por chave,
presets de proporção, caminho visível no overlay — inspirado em Pencil2D 0.7,
OpenToonz e Procreate Dreams.

## Próximos passos

- **Primeiro build real** no Android Studio (NDK 26.3 + CMake 3.22) e correr os
  erros que o sandbox não pega (Gradle, JNI, Kotlin).
- Autoload só na primeira criação (ver ponto de atenção acima).
- Gerenciador de múltiplos projetos; exportação; onion skinning; seletor de cor
  livre; opacidade do pincel na UI; cross-fade entre keyframes; undo estrutural;
  estilo visual dos painéis.

## Concorrência

Toda estrutura compartilhada (`Timeline`, índices, pincel, undo/redo, câmera, fila de
deleção GL) é protegida por `timelineMutex_`; `playing_` é `std::atomic<bool>`.

## Build

- Android Studio Koala+ com NDK 26.3.11579264 e CMake 3.22.1.
- `minSdk 26`, `compileSdk`/`targetSdk 34`.

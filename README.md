# Traço Dreams (DreamMotion)

Versão Android inspirada no Procreate Dreams: desenho em camadas + timeline de animação
com keyframes, câmera virtual e agora **projeto salvável**. Esqueleto funcional em
evolução, não o app completo.

## Arquitetura

- **Engine em C++** (`app/src/main/cpp`): estado de desenho (traços, camadas, frames,
  timeline, pincel, undo/redo, câmera, salvar/carregar) e renderização OpenGL ES 3
  em thread nativa dedicada com contexto EGL próprio.
- **Ponte JNI** (`native_bridge.cpp` + `NativeEngine.kt`).
- **UI Android** (Views programáticas): `DreamsSurfaceView`, `ProjectPanel`
  (salvar/carregar), `BrushPanel`, `LayersPanel`, `CameraPanel`.

## Salvar / carregar projeto

- Formato: JSON próprio (`app/src/main/cpp/include/json.h` + `src/json.cpp`), sem
  dependência externa — o NDK não traz parser JSON pronto e não há acesso à rede
  no ambiente de build para puxar uma lib. O parser/writer foi testado por
  round-trip (incluindo cores ARGB com o bit de sinal ligado, caso propenso a
  erro ao passar por `double`) antes de entrar no repositório.
- O que é salvo: framerate, frame atual, todos os frames (tipo, duração, transform,
  easing) com suas camadas (nome, visibilidade, opacidade, blend mode) e traços
  (cor, parâmetros de pincel, todos os pontos com pressão), a trilha de câmera
  inteira (chaves com pose/easing/hold) e o pincel atualmente selecionado.
- **Autosave**: `MainActivity.onPause()` chama `nativeSaveProject` — minimizar ou
  fechar o app não depende do usuário tocar em "Salvar".
- **Autoload**: `DreamsSurfaceView.surfaceCreated` chama `nativeLoadProject` assim
  que a engine nativa existe; se o arquivo ainda não existir (primeira execução),
  é um no-op silencioso, não um erro.
- Botões explícitos de Salvar/Carregar no `ProjectPanel`, para controle manual.
- **Um único slot** (`files/current_project.json`) — sem gerenciador de múltiplos
  projetos ainda.

### Limitação honesta importante

Carregar um projeto (ou remover uma camada) substitui os objetos `Layer` antigos
sem liberar as texturas/FBOs OpenGL que eles tinham alocado — os handles ficam
órfãos na GPU. Isso já era verdade para `removeLayer` antes deste commit; carregar
projeto só tornou o problema mais visível (pode acontecer várias vezes numa
sessão). Corrigir exige coletar os handles antigos e chamar
`glDeleteTextures`/`glDeleteFramebuffers` na render thread antes de descartá-los —
fica documentado aqui como dívida técnica real, não escondido.

## Pipeline de renderização

1. Cada camada suja é re-rasterizada no seu FBO.
2. Todas as camadas são compostas numa textura de cena (tamanho do canvas).
3. A cena é apresentada: vista de edição (canvas inteiro + overlay da câmera) ou
   vista da câmera (letterbox, desenho desativado).

## Câmera (resumo — detalhes no histórico de commits)

Objeto de cena com chaves por frame (posição/zoom/rotação), easing e hold por
chave, presets de proporção, caminho visível no overlay — inspirado em Pencil2D
0.7, OpenToonz e Procreate Dreams.

## O que já funciona (resumo)

Desenho com pincel (cor/tamanho/dureza/borracha), undo/redo de traços, camadas
com blend modes, frames e playback, keyframes de objeto com interpolação real,
câmera virtual, e agora salvar/carregar projeto com autosave/autoload.

## Próximos passos

- Corrigir o vazamento de recursos GL descrito acima.
- Gerenciador de múltiplos projetos/arquivos (hoje é um único slot).
- Exportação, onion skinning, seletor de cor livre, opacidade do pincel na UI,
  cross-fade entre keyframes, undo estrutural, estilo visual dos painéis.

## Concorrência

Toda estrutura compartilhada (`Timeline`, índices, pincel, undo/redo, câmera) é
protegida por `timelineMutex_`; `playing_` é `std::atomic<bool>`. Save/load só
tocam dados de CPU sob esse mesmo mutex, então são seguros de chamar de qualquer
thread.

## Build

- Android Studio Koala+ com NDK 26.3.11579264 e CMake 3.22.1.
- `minSdk 26`, `compileSdk`/`targetSdk 34`.

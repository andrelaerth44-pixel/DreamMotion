#pragma once
#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <string>
#include <chrono>
#include "timeline.h"

namespace dreams {

struct RenderCommand {
    enum class Kind { AddPoint, BeginStroke, EndStroke, Resize, Shutdown } kind;
    DrawPoint point;
    float width = 0, height = 0;
};

// Registro de undo: aponta para onde um traço completo foi desenhado (índice de
// frame + índice de camada). O traço em si permanece na Layer até undo() ser
// chamado; só então é removido e guardado em RedoRecord para um possível redo.
struct UndoRecord {
    int frameIndex;
    int layerIndex;
};

struct RedoRecord {
    int frameIndex;
    int layerIndex;
    std::unique_ptr<Stroke> stroke;
};

// Motor de renderização OpenGL ES 3. Ver revisões anteriores para compositing,
// VBO persistente, playback, interpolação de keyframes e pincel configurável.
// Esta revisão adiciona undo/redo em nível de traço: cada traço completo (não
// cada ponto) vira uma entrada na pilha de undo; iniciar um traço novo limpa a
// pilha de redo (comportamento padrão de qualquer editor).
class GLRenderEngine {
public:
    GLRenderEngine();
    ~GLRenderEngine();

    void start(ANativeWindow* window);
    void stop();

    void onSurfaceResized(int widthPx, int heightPx);
    void onTouchDown(float x, float y, float pressure);
    void onTouchMove(float x, float y, float pressure);
    void onTouchUp();

    // --- Pincel atual ---
    void setBrushColor(uint32_t argb);
    void setBrushSize(float px);
    void setBrushHardness(float h);
    void setEraserMode(bool enabled);
    bool isEraserMode();

    // --- Undo / redo (em nível de traço) ---
    void undo();
    void redo();
    bool canUndo();
    bool canRedo();

    // --- Camadas do frame corrente ---
    int layerCount();
    std::string layerName(int index);
    bool layerVisible(int index);
    float layerOpacity(int index);
    void setLayerVisible(int index, bool visible);
    void setLayerOpacity(int index, float opacity);
    void setActiveLayer(int index);
    void addLayer(const std::string& name);
    void removeLayer(int index);
    void moveLayer(int fromIndex, int toIndex);

    // --- Timeline ---
    int frameCount();
    int currentFrameIndex();
    void addFrame();
    void goToFrame(int index);

    // --- Keyframes / autoria de transform ---
    int frameType(int index);
    void setFrameType(int index, int type);
    void nudgeFrameTransform(int index, float dTx, float dTy, float dScale, float dRotationDeg);
    void appendInterpolatedFrame();

    // --- Playback ---
    void play();
    void pause();
    bool isPlaying();

private:
    void renderLoop();
    bool initEGL(ANativeWindow* window);
    void destroyEGL();
    void advancePlayback(double deltaMs);

    void drawFrame();
    void ensureLayerTarget(Layer& layer);
    void renderLayerContents(Layer& layer);
    void compositeLayer(const Layer& layer, const Transform& transform);

    void ensureStrokeVboCapacity(size_t requiredBytes);
    static void appendStrokeQuadVertices(std::vector<float>& out, const Stroke& stroke);

    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    ANativeWindow* window_ = nullptr;

    int width_ = 0, height_ = 0;
    GLuint strokeProgram_ = 0;
    GLuint compositeProgram_ = 0;
    GLuint fullscreenQuadVbo_ = 0;

    GLuint strokeVbo_ = 0;
    size_t strokeVboCapacityBytes_ = 0;
    std::vector<float> strokeVertexScratch_;

    std::mutex timelineMutex_;
    Timeline timeline_{24};
    int currentFrameIndex_ = 0;
    int activeLayerIndex_ = 0;

    uint32_t currentColorArgb_ = 0xFF202020;
    Brush currentBrush_{};

    std::vector<UndoRecord> undoRecords_;
    std::vector<RedoRecord> redoRecords_;
    int pendingUndoFrameIndex_ = -1;
    int pendingUndoLayerIndex_ = -1;

    std::atomic<bool> playing_{false};
    std::chrono::steady_clock::time_point lastFrameTime_{};
    double frameAccumulatorMs_ = 0.0;

    Stroke* activeStroke_ = nullptr;
    Layer* activeLayer_ = nullptr;
    uint64_t nextStrokeId_ = 1;

    std::thread renderThread_;
    std::atomic<bool> running_{false};
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::deque<RenderCommand> commandQueue_;
};

} // namespace dreams

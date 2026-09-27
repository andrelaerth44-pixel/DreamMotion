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

// Motor de renderização OpenGL ES 3 com thread de render dedicada, compositing
// por camada via FBO, VBO persistente de traços, e API de estrutura (camadas +
// frames) protegida por timelineMutex_. Esta revisão adiciona playback: quando
// playing_ está ativo, a própria render thread avança currentFrameIndex_ no
// ritmo do framerate/holdDurationTicks de cada Frame, sem depender de nenhum
// timer do lado Kotlin.
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

    // --- Playback ---
    void play();
    void pause();
    bool isPlaying();

private:
    void renderLoop();
    bool initEGL(ANativeWindow* window);
    void destroyEGL();
    void advancePlayback(double deltaMs); // chamada pela render thread a cada iteração

    void drawFrame();
    void ensureLayerTarget(Layer& layer);
    void renderLayerContents(Layer& layer);
    void compositeLayer(const Layer& layer);

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

    // Playback: só tocado pela render thread, exceto playing_ (atomic, lido/
    // escrito também pela UI thread via play()/pause()/isPlaying()).
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

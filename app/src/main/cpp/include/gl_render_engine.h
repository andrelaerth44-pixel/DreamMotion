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
#include "timeline.h"

namespace dreams {

struct RenderCommand {
    enum class Kind { AddPoint, BeginStroke, EndStroke, Resize, Shutdown } kind;
    DrawPoint point;
    float width = 0, height = 0;
};

// Motor de renderização OpenGL ES 3 com thread de render dedicada. Ver histórico
// de comentários anteriores para o desenho de compositing por FBO e do VBO
// persistente de traços. Esta revisão adiciona a API de estrutura (camadas e
// frames) que a UI Kotlin usa para expor um painel de camadas + navegação de
// timeline. Toda essa API é protegida por timelineMutex_, porque é chamada
// diretamente da UI thread (via JNI) enquanto a render thread pode estar
// iterando a mesma estrutura em drawFrame().
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
    void setActiveLayer(int index); // camada que recebe novos traços
    void addLayer(const std::string& name);
    void removeLayer(int index);
    void moveLayer(int fromIndex, int toIndex);

    // --- Timeline ---
    int frameCount();
    int currentFrameIndex();
    void addFrame();
    void goToFrame(int index);

private:
    void renderLoop();
    bool initEGL(ANativeWindow* window);
    void destroyEGL();

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

    // Protege timeline_, currentFrameIndex_ e activeLayerIndex_ — tocados tanto
    // pela render thread (renderLoop/drawFrame) quanto pela UI thread (métodos
    // públicos acima, chamados via JNI a partir do painel de camadas).
    std::mutex timelineMutex_;
    Timeline timeline_{24};
    int currentFrameIndex_ = 0;
    int activeLayerIndex_ = 0;

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

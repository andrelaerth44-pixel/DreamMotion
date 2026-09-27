#pragma once
#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <deque>
#include "timeline.h"

namespace dreams {

// Comando enfileirado pela thread de UI (via JNI) e consumido pela thread de
// render nativa dedicada.
struct RenderCommand {
    enum class Kind { AddPoint, BeginStroke, EndStroke, Resize, Shutdown } kind;
    DrawPoint point;
    float width = 0, height = 0;
};

// Motor de renderização OpenGL ES 3, dono de sua própria EGL context/surface e de
// uma thread de render dedicada — evita competir com a UI thread do Android, que
// é o ganho de performance de mover a engine para C++/nativo em vez de GLSurfaceView.
//
// Compositing: cada Layer visível é renderizada para sua própria textura/FBO
// (ensureLayerTarget + renderLayerContents) e depois composta no framebuffer
// padrão como um quad texturizado (compositeLayer), com o blend mode da camada.
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

    Timeline& timeline() { return timeline_; }

private:
    void renderLoop();
    bool initEGL(ANativeWindow* window);
    void destroyEGL();

    void drawFrame();
    void ensureLayerTarget(Layer& layer);
    void renderLayerContents(Layer& layer);   // redesenha todos os traços da camada no seu FBO
    void compositeLayer(const Layer& layer);  // desenha a textura da camada no framebuffer padrão
    void drawStrokeQuads(const Stroke& stroke, GLint colorLoc, GLint hardnessLoc);

    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    ANativeWindow* window_ = nullptr;

    int width_ = 0, height_ = 0;
    GLuint strokeProgram_ = 0;
    GLuint compositeProgram_ = 0;
    GLuint fullscreenQuadVbo_ = 0;

    Timeline timeline_{24};
    Stroke* activeStroke_ = nullptr; // ponteiro bruto: dono é a Layer corrente
    Layer* activeLayer_ = nullptr;   // camada que contém activeStroke_, para marcar dirty por ponto
    uint64_t nextStrokeId_ = 1;

    std::thread renderThread_;
    std::atomic<bool> running_{false};
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::deque<RenderCommand> commandQueue_;
};

} // namespace dreams

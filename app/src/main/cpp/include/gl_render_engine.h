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
// uma thread de render dedicada — evita competir com a UI thread do Android.
//
// Compositing: cada Layer visível é renderizada para sua própria textura/FBO
// (ensureLayerTarget + renderLayerContents) e depois composta no framebuffer
// padrão como um quad texturizado (compositeLayer), com o blend mode da camada.
//
// Geometria de traço: um VBO persistente (strokeVbo_) é reaproveitado entre
// desenhos — a geometria de um traço inteiro (todos os seus carimbos) é
// montada em um std::vector no lado C++ e enviada em um único glBufferSubData
// + um único glDrawArrays, em vez de criar/destruir um VBO por ponto.
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

    GLuint strokeVbo_ = 0;               // persistente, reaproveitado por traço/camada
    size_t strokeVboCapacityBytes_ = 0;  // cresce (dobrando) sob demanda, nunca encolhe
    std::vector<float> strokeVertexScratch_; // buffer CPU reaproveitado entre chamadas (evita realocar a cada traço)

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

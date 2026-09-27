#include "gl_render_engine.h"
#include <android/log.h>
#include <chrono>

#define LOG_TAG "DreamsGL"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace dreams {
namespace {

const char* kStrokeVertexShader = R"(#version 300 es
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aLocalUV;
uniform vec2 uViewportSize;
out vec2 vLocalUV;
void main() {
    vLocalUV = aLocalUV;
    vec2 ndc = (aPosition / uViewportSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
)";

// Carimbo circular com borda suave controlada por uHardness — a base de todo pincel.
const char* kStrokeFragmentShader = R"(#version 300 es
precision mediump float;
in vec2 vLocalUV;
uniform vec4 uColor;
uniform float uHardness;
out vec4 fragColor;
void main() {
    float d = length(vLocalUV);
    float alpha = 1.0 - smoothstep(uHardness, 1.0, d);
    if (alpha <= 0.001) discard;
    fragColor = vec4(uColor.rgb, uColor.a * alpha);
}
)";

const char* kCompositeVertexShader = R"(#version 300 es
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)";

const char* kCompositeFragmentShader = R"(#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTexture;
uniform float uOpacity;
out vec4 fragColor;
void main() {
    vec4 c = texture(uTexture, vUV);
    fragColor = vec4(c.rgb, c.a * uOpacity);
}
)";

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        LOGE("Falha ao compilar shader: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint linkProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) return 0;
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        LOGE("Falha ao linkar programa: %s", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

} // namespace

GLRenderEngine::GLRenderEngine() = default;
GLRenderEngine::~GLRenderEngine() { stop(); }

void GLRenderEngine::start(ANativeWindow* window) {
    if (running_.exchange(true)) return;
    window_ = window;
    renderThread_ = std::thread(&GLRenderEngine::renderLoop, this);
}

void GLRenderEngine::stop() {
    if (!running_.exchange(false)) return;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        commandQueue_.push_back({RenderCommand::Kind::Shutdown});
    }
    queueCv_.notify_all();
    if (renderThread_.joinable()) renderThread_.join();
}

void GLRenderEngine::onSurfaceResized(int widthPx, int heightPx) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    commandQueue_.push_back({RenderCommand::Kind::Resize, {}, (float) widthPx, (float) heightPx});
    queueCv_.notify_all();
}

void GLRenderEngine::onTouchDown(float x, float y, float pressure) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    commandQueue_.push_back({RenderCommand::Kind::BeginStroke, DrawPoint{x, y, pressure}});
    queueCv_.notify_all();
}

void GLRenderEngine::onTouchMove(float x, float y, float pressure) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    commandQueue_.push_back({RenderCommand::Kind::AddPoint, DrawPoint{x, y, pressure}});
    queueCv_.notify_all();
}

void GLRenderEngine::onTouchUp() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    commandQueue_.push_back({RenderCommand::Kind::EndStroke});
    queueCv_.notify_all();
}

bool GLRenderEngine::initEGL(ANativeWindow* window) {
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) { LOGE("eglGetDisplay falhou"); return false; }
    eglInitialize(display_, nullptr, nullptr);

    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };
    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(display_, configAttribs, &config, 1, &numConfigs);
    if (numConfigs == 0) { LOGE("Nenhum EGLConfig compatível"); return false; }

    EGLint format;
    eglGetConfigAttrib(display_, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(window, 0, 0, format);

    surface_ = eglCreateWindowSurface(display_, config, window, nullptr);
    const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, contextAttribs);
    if (!eglMakeCurrent(display_, surface_, surface_, context_)) {
        LOGE("eglMakeCurrent falhou");
        return false;
    }

    eglQuerySurface(display_, surface_, EGL_WIDTH, &width_);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height_);

    strokeProgram_ = linkProgram(kStrokeVertexShader, kStrokeFragmentShader);
    compositeProgram_ = linkProgram(kCompositeVertexShader, kCompositeFragmentShader);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return strokeProgram_ != 0 && compositeProgram_ != 0;
}

void GLRenderEngine::destroyEGL() {
    if (display_ == EGL_NO_DISPLAY) return;
    eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (context_ != EGL_NO_CONTEXT) eglDestroyContext(display_, context_);
    if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_, surface_);
    eglTerminate(display_);
    display_ = EGL_NO_DISPLAY;
    surface_ = EGL_NO_SURFACE;
    context_ = EGL_NO_CONTEXT;
    if (window_) {
        ANativeWindow_release(window_);
        window_ = nullptr;
    }
}

void GLRenderEngine::renderLoop() {
    if (!initEGL(window_)) {
        LOGE("Inicialização EGL falhou — encerrando thread de render");
        running_ = false;
        return;
    }
    LOGI("Render thread nativa iniciada (%dx%d)", width_, height_);

    while (running_) {
        std::deque<RenderCommand> batch;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait_for(lock, std::chrono::milliseconds(16),
                               [this] { return !commandQueue_.empty(); });
            batch.swap(commandQueue_);
        }

        bool shouldStop = false;
        for (auto& cmd : batch) {
            switch (cmd.kind) {
                case RenderCommand::Kind::Resize:
                    width_ = (int) cmd.width; height_ = (int) cmd.height;
                    glViewport(0, 0, width_, height_);
                    break;
                case RenderCommand::Kind::BeginStroke: {
                    Frame* frame = timeline_.frameAt(0);
                    Layer* layer = (frame && !frame->layers.empty()) ? frame->layers[0].get() : nullptr;
                    if (layer) {
                        auto stroke = std::make_unique<Stroke>(nextStrokeId_++, Brush{}, 0xFF202020);
                        stroke->addPoint(cmd.point);
                        activeStroke_ = stroke.get();
                        layer->addStroke(std::move(stroke));
                    }
                    break;
                }
                case RenderCommand::Kind::AddPoint:
                    if (activeStroke_) activeStroke_->addPoint(cmd.point);
                    break;
                case RenderCommand::Kind::EndStroke:
                    if (activeStroke_) { activeStroke_->finish(); activeStroke_ = nullptr; }
                    break;
                case RenderCommand::Kind::Shutdown:
                    shouldStop = true;
                    break;
            }
        }

        drawFrame();
        eglSwapBuffers(display_, surface_);

        if (shouldStop) break;
    }

    destroyEGL();
    LOGI("Render thread nativa encerrada");
}

void GLRenderEngine::drawFrame() {
    glClearColor(0.93f, 0.93f, 0.93f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Estado atual: desenha direto no framebuffer padrão usando o programa de
    // stroke, um quad por ponto do traço (sem compor via FBO por camada ainda —
    // ver renderStrokeToLayer/compositeLayer para o próximo passo).
    Frame* frame = timeline_.frameAt(0);
    if (!frame) return;

    glUseProgram(strokeProgram_);
    GLint viewportLoc = glGetUniformLocation(strokeProgram_, "uViewportSize");
    glUniform2f(viewportLoc, (float) width_, (float) height_);
    GLint colorLoc = glGetUniformLocation(strokeProgram_, "uColor");
    GLint hardnessLoc = glGetUniformLocation(strokeProgram_, "uHardness");

    for (auto& layer : frame->layers) {
        if (!layer->visible) continue;
        for (auto& strokePtr : layer->strokes()) {
            const Stroke& stroke = *strokePtr;
            float r = ((stroke.color() >> 16) & 0xFF) / 255.f;
            float g = ((stroke.color() >> 8) & 0xFF) / 255.f;
            float b = (stroke.color() & 0xFF) / 255.f;
            float a = ((stroke.color() >> 24) & 0xFF) / 255.f * stroke.brush().opacity;
            glUniform4f(colorLoc, r, g, b, a);
            glUniform1f(hardnessLoc, stroke.brush().hardness);

            for (const auto& p : stroke.points()) {
                float size = stroke.brush().baseSizePx *
                    (stroke.brush().pressureAffectsSize
                         ? (stroke.brush().minSizeFactor + (1.f - stroke.brush().minSizeFactor) * p.pressure)
                         : 1.f);
                float half = size * 0.5f;
                // Quad em torno do ponto; UV local em [-1,1] usado no fragment
                // shader para desenhar o carimbo circular com borda suave.
                float verts[] = {
                    p.x - half, p.y - half, -1, -1,
                    p.x + half, p.y - half,  1, -1,
                    p.x - half, p.y + half, -1,  1,
                    p.x + half, p.y + half,  1,  1,
                };
                // NOTA DE PERFORMANCE: um VBO por ponto por frame é simples e
                // correto, mas está longe do ideal — o próximo passo de
                // performance é um VBO persistente (buffer circular) ou
                // instancing em vez de gen/delete a cada carimbo.
                GLuint vbo;
                glGenBuffers(1, &vbo);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*) 0);
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*) (2 * sizeof(float)));
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                glDeleteBuffers(1, &vbo);
            }
        }
    }
}

void GLRenderEngine::compositeLayer(const Layer&) { /* TODO: compositing via FBO por camada */ }
void GLRenderEngine::renderStrokeToLayer(const Stroke&, const Layer&) { /* TODO */ }

} // namespace dreams

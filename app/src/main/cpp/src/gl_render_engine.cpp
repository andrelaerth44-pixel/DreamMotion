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

// Aplica o blend fixo-function mais próximo do blend mode da camada. Multiply/Screen
// são aproximações via fixed-function blend (funcionam bem na prática); um blend
// "correto" por pixel exigiria um shader de blend dedicado — fica como TODO futuro.
void applyLayerBlendMode(BlendMode mode) {
    switch (mode) {
        case BlendMode::Multiply:
            glBlendFunc(GL_DST_COLOR, GL_ZERO);
            break;
        case BlendMode::Screen:
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
            break;
        case BlendMode::Add:
            glBlendFunc(GL_ONE, GL_ONE);
            break;
        case BlendMode::Normal:
        case BlendMode::Erase:
        default:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
    }
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

    // Quad de tela cheia para o composite: posição NDC + UV. V é invertido (1-v)
    // porque a textura da camada foi preenchida com a mesma convenção de pixel-
    // para-NDC usada no framebuffer padrão (ver kStrokeVertexShader); ao amostrar
    // de volta como textura isso equivale a uma inversão vertical que precisa ser
    // compensada aqui. Vale reconferir isso visualmente ao rodar em dispositivo real.
    float quad[] = {
        -1, -1,  0, 1,
         1, -1,  1, 1,
        -1,  1,  0, 0,
         1,  1,  1, 0,
    };
    glGenBuffers(1, &fullscreenQuadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, fullscreenQuadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

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
                    // Camadas já alocadas têm FBOs no tamanho antigo; a forma mais
                    // simples de lidar com resize por ora é forçar realocação.
                    if (Frame* f = timeline_.frameAt(0)) {
                        for (auto& l : f->layers) { l->textureHandle = -1; l->fboHandle = -1; l->dirty = true; }
                    }
                    break;
                case RenderCommand::Kind::BeginStroke: {
                    Frame* frame = timeline_.frameAt(0);
                    Layer* layer = (frame && !frame->layers.empty()) ? frame->layers[0].get() : nullptr;
                    if (layer) {
                        auto stroke = std::make_unique<Stroke>(nextStrokeId_++, Brush{}, 0xFF202020);
                        stroke->addPoint(cmd.point);
                        activeStroke_ = stroke.get();
                        activeLayer_ = layer;
                        layer->addStroke(std::move(stroke)); // já marca layer->dirty = true
                    }
                    break;
                }
                case RenderCommand::Kind::AddPoint:
                    if (activeStroke_) {
                        activeStroke_->addPoint(cmd.point);
                        if (activeLayer_) activeLayer_->dirty = true;
                    }
                    break;
                case RenderCommand::Kind::EndStroke:
                    if (activeStroke_) activeStroke_->finish();
                    activeStroke_ = nullptr;
                    activeLayer_ = nullptr;
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

void GLRenderEngine::ensureLayerTarget(Layer& layer) {
    if (layer.textureHandle != -1) return;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("FBO da camada '%s' incompleto", layer.name.c_str());
    }

    layer.textureHandle = (int) tex;
    layer.fboHandle = (int) fbo;
    layer.dirty = true;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLRenderEngine::drawStrokeQuads(const Stroke& stroke, GLint colorLoc, GLint hardnessLoc) {
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
        float verts[] = {
            p.x - half, p.y - half, -1, -1,
            p.x + half, p.y - half,  1, -1,
            p.x - half, p.y + half, -1,  1,
            p.x + half, p.y + half,  1,  1,
        };
        // NOTA DE PERFORMANCE: um VBO por ponto por frame é simples e correto,
        // mas está longe do ideal — o próximo passo óbvio é um VBO persistente
        // (buffer circular) ou instancing em vez de gen/delete a cada carimbo.
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

void GLRenderEngine::renderLayerContents(Layer& layer) {
    ensureLayerTarget(layer);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) layer.fboHandle);
    glViewport(0, 0, width_, height_);
    glClearColor(0.f, 0.f, 0.f, 0.f); // camada começa transparente, não branca
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(strokeProgram_);
    glUniform2f(glGetUniformLocation(strokeProgram_, "uViewportSize"), (float) width_, (float) height_);
    GLint colorLoc = glGetUniformLocation(strokeProgram_, "uColor");
    GLint hardnessLoc = glGetUniformLocation(strokeProgram_, "uHardness");

    for (auto& strokePtr : layer.strokes()) {
        const Stroke& stroke = *strokePtr;
        if (stroke.brush().blendMode == BlendMode::Erase) {
            // Borracha: reduz o alpha já presente na camada em vez de somar cor.
            glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        drawStrokeQuads(stroke, colorLoc, hardnessLoc);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLRenderEngine::compositeLayer(const Layer& layer) {
    glUseProgram(compositeProgram_);
    applyLayerBlendMode(layer.blendMode);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint) layer.textureHandle);
    glUniform1i(glGetUniformLocation(compositeProgram_, "uTexture"), 0);
    glUniform1f(glGetUniformLocation(compositeProgram_, "uOpacity"), layer.opacity);

    glBindBuffer(GL_ARRAY_BUFFER, fullscreenQuadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*) (2 * sizeof(float)));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void GLRenderEngine::drawFrame() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
    glClearColor(0.93f, 0.93f, 0.93f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    Frame* frame = timeline_.frameAt(0);
    if (!frame) return;

    for (auto& layerPtr : frame->layers) {
        Layer& layer = *layerPtr;
        if (!layer.visible) continue;
        ensureLayerTarget(layer);
        if (layer.dirty) {
            renderLayerContents(layer); // redesenha todos os traços da camada no seu FBO
            layer.dirty = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width_, height_);
        compositeLayer(layer);
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // restaura o blend padrão
}

} // namespace dreams

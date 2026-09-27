#include "gl_render_engine.h"
#include <android/log.h>

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
layout(location = 0) in vec2 aUnit;
layout(location = 1) in vec2 aUV;
uniform vec2 uViewportSize;
uniform vec2 uTranslate;
uniform float uScale;
uniform float uRotationRad;
out vec2 vUV;
void main() {
    vUV = aUV;
    vec2 pixelPos = aUnit * uViewportSize;
    vec2 center = uViewportSize * 0.5;
    vec2 p = pixelPos - center;
    float c = cos(uRotationRad);
    float s = sin(uRotationRad);
    vec2 rotated = vec2(p.x * c - p.y * s, p.x * s + p.y * c) * uScale;
    vec2 finalPos = rotated + center + uTranslate;
    vec2 ndc = (finalPos / uViewportSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
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

void applyLayerBlendMode(BlendMode mode) {
    switch (mode) {
        case BlendMode::Multiply: glBlendFunc(GL_DST_COLOR, GL_ZERO); break;
        case BlendMode::Screen: glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR); break;
        case BlendMode::Add: glBlendFunc(GL_ONE, GL_ONE); break;
        case BlendMode::Normal:
        case BlendMode::Erase:
        default: glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;
    }
}

constexpr float kDegToRad = 0.017453292519943295f;

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

// --- Pincel atual ---

void GLRenderEngine::setBrushColor(uint32_t argb) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    currentColorArgb_ = argb;
}

void GLRenderEngine::setBrushSize(float px) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    currentBrush_.baseSizePx = px;
}

void GLRenderEngine::setBrushHardness(float h) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    currentBrush_.hardness = h;
}

void GLRenderEngine::setEraserMode(bool enabled) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    currentBrush_.blendMode = enabled ? BlendMode::Erase : BlendMode::Normal;
}

bool GLRenderEngine::isEraserMode() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    return currentBrush_.blendMode == BlendMode::Erase;
}

// --- Undo / redo ---

void GLRenderEngine::undo() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    if (undoRecords_.empty()) return;
    UndoRecord rec = undoRecords_.back();
    undoRecords_.pop_back();

    Frame* f = timeline_.frameAt((size_t) rec.frameIndex);
    if (!f || rec.layerIndex < 0 || rec.layerIndex >= (int) f->layers.size()) return; // registro obsoleto (camada/frame removido), descarta
    Layer* layer = f->layers[rec.layerIndex].get();
    auto stroke = layer->popLastStroke();
    if (!stroke) return;
    if (activeLayer_ == layer && activeStroke_ == stroke.get()) {
        // Estava desenhando exatamente esse traço quando o undo chegou; caso raro
        // (ex.: botão de undo tocado no meio de um gesto), mas evita ponteiro solto.
        activeStroke_ = nullptr;
        activeLayer_ = nullptr;
    }
    redoRecords_.push_back({rec.frameIndex, rec.layerIndex, std::move(stroke)});
}

void GLRenderEngine::redo() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    if (redoRecords_.empty()) return;
    RedoRecord rec = std::move(redoRecords_.back());
    redoRecords_.pop_back();

    Frame* f = timeline_.frameAt((size_t) rec.frameIndex);
    if (!f || rec.layerIndex < 0 || rec.layerIndex >= (int) f->layers.size()) return;
    Layer* layer = f->layers[rec.layerIndex].get();
    layer->restoreStroke(std::move(rec.stroke));
    undoRecords_.push_back({rec.frameIndex, rec.layerIndex});
}

bool GLRenderEngine::canUndo() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    return !undoRecords_.empty();
}

bool GLRenderEngine::canRedo() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    return !redoRecords_.empty();
}

// --- Camadas ---

int GLRenderEngine::layerCount() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    Frame* f = timeline_.frameAt((size_t) currentFrameIndex_);
    return f ? (int) f->layers.size() : 0;
}

std::string GLRenderEngine::layerName(int index) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    Frame* f = timeline_.frameAt((size_t) currentFrameIndex_);
    if (!f || index < 0 || index >= (int) f->layers.size()) return "";
    return f->layers[index]->name;
}

bool GLRenderEngine::layerVisible(int index) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    Frame* f = timeline_.frameAt((size_t) currentFrameIndex_);
    if (!f || index < 0 || index >= (int) f->layers.size()) return false;
    return f->layers[index]->visible;
}

float GLRenderEngine::layerOpacity(int index) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    Frame* f = timeline_.frameAt((size_t) currentFrameIndex_);
    if (!f || index < 0 || index >= (int) f->layers.size()) return 1.f;
    return f->layers[index]->opacity;
}

void GLRenderEngine::setLayerVisible(int index, bool visible) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    Frame* f = timeline_.frameAt((size_t) currentFrameIndex_);
    if (!f || index < 0 || index >= (int) f->layers.size()) return;
    f->layers[index]->visible = visible;
}

void GLRenderEngine::setLayerOpacity(int index, float opacity) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    Frame* f = timeline_.frameAt((size_t) currentFrameIndex_);
    if (!f || index < 0 || index >= (int) f->layers.size()) return;
    f->layers[index]->opacity = opacity;
}

void GLRenderEngine::setActiveLayer(int index) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    activeLayerIndex_ = index;
}

void GLRenderEngine::addLayer(const std::string& name) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    Frame* f = timeline_.frameAt((size_t) currentFrameIndex_);
    if (!f) return;
    f->layers.push_back(std::make_unique<Layer>(name));
    activeLayerIndex_ = (int) f->layers.size() - 1;
}

void GLRenderEngine::removeLayer(int index) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    Frame* f = timeline_.frameAt((size_t) currentFrameIndex_);
    if (!f || index < 0 || index >= (int) f->layers.size()) return;
    if (f->layers.size() <= 1) return;
    if (activeStroke_ != nullptr && activeLayer_ == f->layers[index].get()) {
        activeStroke_ = nullptr;
        activeLayer_ = nullptr;
    }
    f->layers.erase(f->layers.begin() + index);
    if (activeLayerIndex_ >= (int) f->layers.size()) activeLayerIndex_ = (int) f->layers.size() - 1;
}

void GLRenderEngine::moveLayer(int fromIndex, int toIndex) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    Frame* f = timeline_.frameAt((size_t) currentFrameIndex_);
    if (!f) return;
    int count = (int) f->layers.size();
    if (fromIndex < 0 || fromIndex >= count || toIndex < 0 || toIndex >= count || fromIndex == toIndex) return;
    auto moved = std::move(f->layers[fromIndex]);
    f->layers.erase(f->layers.begin() + fromIndex);
    f->layers.insert(f->layers.begin() + toIndex, std::move(moved));
}

// --- Timeline ---

int GLRenderEngine::frameCount() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    return (int) timeline_.frameCount();
}

int GLRenderEngine::currentFrameIndex() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    return currentFrameIndex_;
}

void GLRenderEngine::addFrame() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    timeline_.appendFrame(FrameType::Drawn);
    currentFrameIndex_ = (int) timeline_.frameCount() - 1;
    activeLayerIndex_ = 0;
    activeStroke_ = nullptr;
    activeLayer_ = nullptr;
}

void GLRenderEngine::goToFrame(int index) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    if (index < 0 || index >= (int) timeline_.frameCount()) return;
    currentFrameIndex_ = index;
    activeLayerIndex_ = 0;
    activeStroke_ = nullptr;
    activeLayer_ = nullptr;
}

// --- Keyframes / autoria de transform ---

int GLRenderEngine::frameType(int index) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    Frame* f = timeline_.frameAt((size_t) index);
    return f ? (int) f->type : 0;
}

void GLRenderEngine::setFrameType(int index, int type) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    Frame* f = timeline_.frameAt((size_t) index);
    if (!f || type < 0 || type > 2) return;
    f->type = (FrameType) type;
}

void GLRenderEngine::nudgeFrameTransform(int index, float dTx, float dTy, float dScale, float dRotationDeg) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    Frame* f = timeline_.frameAt((size_t) index);
    if (!f) return;
    f->transform.translateX += dTx;
    f->transform.translateY += dTy;
    float newScale = f->transform.scale + dScale;
    f->transform.scale = newScale < 0.05f ? 0.05f : newScale;
    f->transform.rotationDeg += dRotationDeg;
}

void GLRenderEngine::appendInterpolatedFrame() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    timeline_.appendFrame(FrameType::Interpolated);
    currentFrameIndex_ = (int) timeline_.frameCount() - 1;
    activeLayerIndex_ = 0;
    activeStroke_ = nullptr;
    activeLayer_ = nullptr;
}

// --- Playback ---

void GLRenderEngine::play() { playing_ = true; }
void GLRenderEngine::pause() { playing_ = false; }
bool GLRenderEngine::isPlaying() { return playing_; }

void GLRenderEngine::advancePlayback(double deltaMs) {
    if (!playing_) return;
    frameAccumulatorMs_ += deltaMs;

    std::lock_guard<std::mutex> lock(timelineMutex_);
    int total = (int) timeline_.frameCount();
    if (total <= 0) return;

    auto holdMsFor = [&](int idx) -> double {
        Frame* f = timeline_.frameAt((size_t) idx);
        int ticks = f ? f->holdDurationTicks : 1;
        if (ticks < 1) ticks = 1;
        return ticks * 1000.0 / timeline_.framerate();
    };

    double holdMs = holdMsFor(currentFrameIndex_);
    int guard = 0;
    while (frameAccumulatorMs_ >= holdMs && guard++ < 1000) {
        frameAccumulatorMs_ -= holdMs;
        currentFrameIndex_ = (currentFrameIndex_ + 1) % total;
        holdMs = holdMsFor(currentFrameIndex_);
    }
}

// --- EGL / render loop ---

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

    float quad[] = {
        0, 0,  0, 1,
        1, 0,  1, 1,
        0, 1,  0, 0,
        1, 1,  1, 0,
    };
    glGenBuffers(1, &fullscreenQuadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, fullscreenQuadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    glGenBuffers(1, &strokeVbo_);

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
    lastFrameTime_ = std::chrono::steady_clock::now();

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
                case RenderCommand::Kind::Resize: {
                    std::lock_guard<std::mutex> lock(timelineMutex_);
                    width_ = (int) cmd.width; height_ = (int) cmd.height;
                    if (Frame* f = timeline_.frameAt((size_t) currentFrameIndex_)) {
                        for (auto& l : f->layers) { l->textureHandle = -1; l->fboHandle = -1; l->dirty = true; }
                    }
                    break;
                }
                case RenderCommand::Kind::BeginStroke: {
                    std::lock_guard<std::mutex> lock(timelineMutex_);
                    Frame* frame = timeline_.frameAt((size_t) currentFrameIndex_);
                    Layer* layer = (frame && activeLayerIndex_ >= 0 && activeLayerIndex_ < (int) frame->layers.size())
                                       ? frame->layers[activeLayerIndex_].get() : nullptr;
                    if (layer && !layer->locked) {
                        auto stroke = std::make_unique<Stroke>(nextStrokeId_++, currentBrush_, currentColorArgb_);
                        stroke->addPoint(cmd.point);
                        activeStroke_ = stroke.get();
                        activeLayer_ = layer;
                        pendingUndoFrameIndex_ = currentFrameIndex_;
                        pendingUndoLayerIndex_ = activeLayerIndex_;
                        redoRecords_.clear(); // nova ação do usuário invalida o redo, como em qualquer editor
                        layer->addStroke(std::move(stroke));
                    }
                    break;
                }
                case RenderCommand::Kind::AddPoint: {
                    std::lock_guard<std::mutex> lock(timelineMutex_);
                    if (activeStroke_) {
                        activeStroke_->addPoint(cmd.point);
                        if (activeLayer_) activeLayer_->dirty = true;
                    }
                    break;
                }
                case RenderCommand::Kind::EndStroke: {
                    std::lock_guard<std::mutex> lock(timelineMutex_);
                    if (activeStroke_) {
                        activeStroke_->finish();
                        undoRecords_.push_back({pendingUndoFrameIndex_, pendingUndoLayerIndex_});
                    }
                    activeStroke_ = nullptr;
                    activeLayer_ = nullptr;
                    break;
                }
                case RenderCommand::Kind::Shutdown:
                    shouldStop = true;
                    break;
            }
        }

        auto now = std::chrono::steady_clock::now();
        double deltaMs = std::chrono::duration<double, std::milli>(now - lastFrameTime_).count();
        lastFrameTime_ = now;
        if (deltaMs > 250.0) deltaMs = 250.0;
        advancePlayback(deltaMs);

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

void GLRenderEngine::ensureStrokeVboCapacity(size_t requiredBytes) {
    if (requiredBytes <= strokeVboCapacityBytes_) return;
    size_t newCapacity = strokeVboCapacityBytes_ == 0 ? 4096 : strokeVboCapacityBytes_;
    while (newCapacity < requiredBytes) newCapacity *= 2;
    glBindBuffer(GL_ARRAY_BUFFER, strokeVbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) newCapacity, nullptr, GL_DYNAMIC_DRAW);
    strokeVboCapacityBytes_ = newCapacity;
}

void GLRenderEngine::appendStrokeQuadVertices(std::vector<float>& out, const Stroke& stroke) {
    for (const auto& p : stroke.points()) {
        float size = stroke.brush().baseSizePx *
            (stroke.brush().pressureAffectsSize
                 ? (stroke.brush().minSizeFactor + (1.f - stroke.brush().minSizeFactor) * p.pressure)
                 : 1.f);
        float half = size * 0.5f;
        float x0 = p.x - half, x1 = p.x + half;
        float y0 = p.y - half, y1 = p.y + half;
        const float quad[] = {
            x0, y0, -1.f, -1.f,
            x1, y0,  1.f, -1.f,
            x0, y1, -1.f,  1.f,

            x0, y1, -1.f,  1.f,
            x1, y0,  1.f, -1.f,
            x1, y1,  1.f,  1.f,
        };
        out.insert(out.end(), std::begin(quad), std::end(quad));
    }
}

void GLRenderEngine::renderLayerContents(Layer& layer) {
    ensureLayerTarget(layer);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) layer.fboHandle);
    glViewport(0, 0, width_, height_);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(strokeProgram_);
    glUniform2f(glGetUniformLocation(strokeProgram_, "uViewportSize"), (float) width_, (float) height_);
    GLint colorLoc = glGetUniformLocation(strokeProgram_, "uColor");
    GLint hardnessLoc = glGetUniformLocation(strokeProgram_, "uHardness");

    glBindBuffer(GL_ARRAY_BUFFER, strokeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*) (2 * sizeof(float)));

    for (auto& strokePtr : layer.strokes()) {
        const Stroke& stroke = *strokePtr;
        if (stroke.points().empty()) continue;

        strokeVertexScratch_.clear();
        appendStrokeQuadVertices(strokeVertexScratch_, stroke);
        if (strokeVertexScratch_.empty()) continue;

        size_t bytes = strokeVertexScratch_.size() * sizeof(float);
        ensureStrokeVboCapacity(bytes);
        glBindBuffer(GL_ARRAY_BUFFER, strokeVbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr) bytes, strokeVertexScratch_.data());

        float r = ((stroke.color() >> 16) & 0xFF) / 255.f;
        float g = ((stroke.color() >> 8) & 0xFF) / 255.f;
        float b = (stroke.color() & 0xFF) / 255.f;
        float a = ((stroke.color() >> 24) & 0xFF) / 255.f * stroke.brush().opacity;
        glUniform4f(colorLoc, r, g, b, a);
        glUniform1f(hardnessLoc, stroke.brush().hardness);

        if (stroke.brush().blendMode == BlendMode::Erase) {
            glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        glDrawArrays(GL_TRIANGLES, 0, (GLsizei) (strokeVertexScratch_.size() / 4));
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLRenderEngine::compositeLayer(const Layer& layer, const Transform& transform) {
    glUseProgram(compositeProgram_);
    applyLayerBlendMode(layer.blendMode);

    glUniform2f(glGetUniformLocation(compositeProgram_, "uViewportSize"), (float) width_, (float) height_);
    glUniform2f(glGetUniformLocation(compositeProgram_, "uTranslate"), transform.translateX, transform.translateY);
    glUniform1f(glGetUniformLocation(compositeProgram_, "uScale"), transform.scale);
    glUniform1f(glGetUniformLocation(compositeProgram_, "uRotationRad"), transform.rotationDeg * kDegToRad);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint) layer.textureHandle);
    glUniform1i(glGetUniformLocation(compositeProgram_, "uTexture"), 0);
    glUniform1f(glGetUniformLocation(compositeProgram_, "uOpacity"), layer.opacity * transform.opacity);

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

    std::lock_guard<std::mutex> lock(timelineMutex_);
    if (currentFrameIndex_ < 0 || currentFrameIndex_ >= (int) timeline_.frameCount()) return;

    Frame* renderFrame = timeline_.contentSourceFrame((size_t) currentFrameIndex_);
    Transform transform = timeline_.resolveTransformForFrameIndex((size_t) currentFrameIndex_);
    if (!renderFrame) return;

    for (auto& layerPtr : renderFrame->layers) {
        Layer& layer = *layerPtr;
        if (!layer.visible) continue;
        ensureLayerTarget(layer);
        if (layer.dirty) {
            renderLayerContents(layer);
            layer.dirty = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width_, height_);
        compositeLayer(layer, transform);
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

} // namespace dreams

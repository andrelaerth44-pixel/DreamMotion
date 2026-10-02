#include "gl_render_engine.h"
#include "json.h"
#include <android/log.h>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>

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

const char* kPresentFragmentShader = R"(#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTexture;
out vec4 fragColor;
void main() {
    fragColor = vec4(texture(uTexture, vUV).rgb, 1.0);
}
)";

const char* kSolidVertexShader = R"(#version 300 es
layout(location = 0) in vec2 aPosition;
uniform vec2 uViewportSize;
void main() {
    vec2 ndc = (aPosition / uViewportSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
)";

const char* kSolidFragmentShader = R"(#version 300 es
precision mediump float;
uniform vec4 uColor;
out vec4 fragColor;
void main() {
    fragColor = uColor;
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
    if (running_.exchange(true)) {
        // Ja estava rodando: a referencia extra do ANativeWindow adquirida pelo
        // chamador JNI nao sera usada por ninguem, entao devolvemos aqui.
        if (window) ANativeWindow_release(window);
        return;
    }
    // Se a render thread anterior terminou sozinha (ex.: falha de EGL), ela ainda
    // esta "joinable"; atribuir uma nova std::thread por cima chamaria terminate().
    if (renderThread_.joinable()) renderThread_.join();
    {
        // Descarta comandos de uma sessao anterior (inclusive um Shutdown que a
        // thread antiga nao chegou a consumir, o que mataria a nova logo no inicio).
        std::lock_guard<std::mutex> lock(queueMutex_);
        commandQueue_.clear();
    }
    window_ = window;
    renderThread_ = std::thread(&GLRenderEngine::renderLoop, this);
}

void GLRenderEngine::stop() {
    bool wasRunning = running_.exchange(false);
    if (wasRunning) {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            commandQueue_.push_back({RenderCommand::Kind::Shutdown});
        }
        queueCv_.notify_all();
    }
    // Sempre faz join se houver thread, mesmo que ela ja tenha encerrado por conta
    // propria (running_ false por falha de EGL): destruir uma std::thread joinable
    // chama terminate().
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
    if (!f || rec.layerIndex < 0 || rec.layerIndex >= (int) f->layers.size()) return;
    Layer* layer = f->layers[rec.layerIndex].get();
    auto stroke = layer->popLastStroke();
    if (!stroke) return;
    if (activeLayer_ == layer && activeStroke_ == stroke.get()) {
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

// --- Camera ---

CameraPose GLRenderEngine::defaultCameraPose() const {
    CameraPose p;
    p.centerX = (float) width_ * 0.5f;
    p.centerY = (float) height_ * 0.5f;
    p.zoom = 1.f;
    p.rotationDeg = 0.f;
    return p;
}

CameraPose GLRenderEngine::resolveCameraPose(int frameIndex) const {
    return cameraTrack_.resolve(frameIndex, defaultCameraPose());
}

float GLRenderEngine::cameraAspectValue() const {
    switch (cameraAspectPreset_) {
        case 1: return 16.f / 9.f;
        case 2: return 4.f / 3.f;
        case 3: return 1.f;
        case 4: return 9.f / 16.f;
        default: return height_ > 0 ? (float) width_ / (float) height_ : 1.f;
    }
}

CameraKey& GLRenderEngine::ensureCameraKeyAtCurrentFrame() {
    if (CameraKey* k = cameraTrack_.keyAt(currentFrameIndex_)) return *k;
    CameraKey key;
    key.frameIndex = currentFrameIndex_;
    key.pose = resolveCameraPose(currentFrameIndex_);
    return cameraTrack_.setKey(key);
}

void GLRenderEngine::setCameraViewMode(bool enabled) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    cameraViewMode_ = enabled;
    if (enabled) { activeStroke_ = nullptr; activeLayer_ = nullptr; }
}

bool GLRenderEngine::isCameraViewMode() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    return cameraViewMode_;
}

void GLRenderEngine::setCameraAspectPreset(int preset) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    cameraAspectPreset_ = (preset < 0 || preset > 4) ? 0 : preset;
}

int GLRenderEngine::cameraAspectPreset() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    return cameraAspectPreset_;
}

void GLRenderEngine::setCameraPathVisible(bool visible) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    cameraPathVisible_ = visible;
}

void GLRenderEngine::setCameraKeyAtCurrentFrame() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    ensureCameraKeyAtCurrentFrame();
}

void GLRenderEngine::removeCameraKeyAtCurrentFrame() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    cameraTrack_.removeKey(currentFrameIndex_);
}

bool GLRenderEngine::hasCameraKeyAtCurrentFrame() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    return cameraTrack_.keyAt(currentFrameIndex_) != nullptr;
}

int GLRenderEngine::cameraKeyCount() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    return (int) cameraTrack_.count();
}

void GLRenderEngine::nudgeCamera(float dx, float dy, float zoomMultiplier, float dRotationDeg) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    CameraKey& k = ensureCameraKeyAtCurrentFrame();
    k.pose.centerX += dx;
    k.pose.centerY += dy;
    float z = k.pose.zoom * zoomMultiplier;
    k.pose.zoom = z < 0.1f ? 0.1f : (z > 20.f ? 20.f : z);
    k.pose.rotationDeg += dRotationDeg;
}

void GLRenderEngine::resetCamera(int what) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    CameraKey& k = ensureCameraKeyAtCurrentFrame();
    CameraPose def = defaultCameraPose();
    switch (what) {
        case 1: k.pose.centerX = def.centerX; k.pose.centerY = def.centerY; break;
        case 2: k.pose.zoom = 1.f; break;
        case 3: k.pose.rotationDeg = 0.f; break;
        default: k.pose = def; break;
    }
}

void GLRenderEngine::setCameraEasing(int easing) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    CameraKey& k = ensureCameraKeyAtCurrentFrame();
    int e = easing < 0 ? 0 : (easing > 3 ? 3 : easing);
    k.easing = (Easing) e;
}

int GLRenderEngine::cameraEasing() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    const CameraKey* k = cameraTrack_.keyAt(currentFrameIndex_);
    return k ? (int) k->easing : (int) Easing::EaseInOut;
}

void GLRenderEngine::setCameraHold(bool hold) {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    CameraKey& k = ensureCameraKeyAtCurrentFrame();
    k.hold = hold;
}

bool GLRenderEngine::cameraHold() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    const CameraKey* k = cameraTrack_.keyAt(currentFrameIndex_);
    return k ? k->hold : false;
}

// --- Recursos GL de camadas descartadas ---

void GLRenderEngine::queueLayerGLResourcesForDeletion(Layer& layer) {
    if (layer.textureHandle != -1) pendingDeleteTextures_.push_back((GLuint) layer.textureHandle);
    if (layer.fboHandle != -1) pendingDeleteFbos_.push_back((GLuint) layer.fboHandle);
    layer.textureHandle = -1;
    layer.fboHandle = -1;
}

void GLRenderEngine::drainPendingGLDeletions() {
    if (!pendingDeleteTextures_.empty()) {
        glDeleteTextures((GLsizei) pendingDeleteTextures_.size(), pendingDeleteTextures_.data());
        pendingDeleteTextures_.clear();
    }
    if (!pendingDeleteFbos_.empty()) {
        glDeleteFramebuffers((GLsizei) pendingDeleteFbos_.size(), pendingDeleteFbos_.data());
        pendingDeleteFbos_.clear();
    }
}

// Roda na render thread logo apos initEGL. Quando a Surface e recriada o contexto
// EGL e novo: todos os handles de textura/FBO/buffer do contexto antigo morreram
// com ele. Nao ha nada a deletar - so esquecer, para que ensureLayerTarget,
// ensureSceneTarget e ensureStrokeVboCapacity realoquem tudo no contexto novo.
// Importante limpar tambem as filas de delecao: ids do contexto antigo poderiam
// coincidir com ids de objetos novos e apagar objetos validos.
void GLRenderEngine::resetGLStateForNewContext() {
    std::lock_guard<std::mutex> lock(timelineMutex_);
    sceneFbo_ = 0;
    sceneTexture_ = 0;
    sceneWidth_ = 0;
    sceneHeight_ = 0;
    strokeVboCapacityBytes_ = 0;
    pendingDeleteTextures_.clear();
    pendingDeleteFbos_.clear();
    for (size_t fi = 0; fi < timeline_.frameCount(); ++fi) {
        Frame* f = timeline_.frameAt(fi);
        if (!f) continue;
        for (auto& l : f->layers) {
            l->textureHandle = -1;
            l->fboHandle = -1;
            l->dirty = true;
        }
    }
    // Gesto interrompido pela perda da Surface: fecha o traco e registra no undo,
    // senao ele ficaria para sempre "em andamento" e fora do historico.
    if (activeStroke_) {
        activeStroke_->finish();
        undoRecords_.push_back({pendingUndoFrameIndex_, pendingUndoLayerIndex_});
    }
    activeStroke_ = nullptr;
    activeLayer_ = nullptr;
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
    queueLayerGLResourcesForDeletion(*f->layers[index]);
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

// --- Keyframes de objeto / autoria de transform ---

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

// --- Projeto (salvar/carregar) ---

bool GLRenderEngine::saveProjectToFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(timelineMutex_);

    json::Value root = json::Value::makeObject();
    root.set("version", json::Value::makeNumber(1));
    root.set("framerate", json::Value::makeNumber(timeline_.framerate()));
    root.set("currentFrameIndex", json::Value::makeNumber(currentFrameIndex_));

    json::Value brushObj = json::Value::makeObject();
    brushObj.set("color", json::Value::makeNumber((double) (int32_t) currentColorArgb_));
    brushObj.set("size", json::Value::makeNumber(currentBrush_.baseSizePx));
    brushObj.set("hardness", json::Value::makeNumber(currentBrush_.hardness));
    root.set("brush", brushObj);

    json::Value framesArr = json::Value::makeArray();
    for (size_t fi = 0; fi < timeline_.frameCount(); ++fi) {
        Frame* f = timeline_.frameAt(fi);
        json::Value fObj = json::Value::makeObject();
        fObj.set("type", json::Value::makeNumber((int) f->type));
        fObj.set("hold", json::Value::makeNumber(f->holdDurationTicks));
        fObj.set("easing", json::Value::makeNumber((int) f->easing));

        json::Value tObj = json::Value::makeObject();
        tObj.set("tx", json::Value::makeNumber(f->transform.translateX));
        tObj.set("ty", json::Value::makeNumber(f->transform.translateY));
        tObj.set("scale", json::Value::makeNumber(f->transform.scale));
        tObj.set("rot", json::Value::makeNumber(f->transform.rotationDeg));
        tObj.set("opacity", json::Value::makeNumber(f->transform.opacity));
        fObj.set("transform", tObj);

        json::Value layersArr = json::Value::makeArray();
        for (auto& layerPtr : f->layers) {
            Layer& layer = *layerPtr;
            json::Value lObj = json::Value::makeObject();
            lObj.set("name", json::Value::makeString(layer.name));
            lObj.set("visible", json::Value::makeBool(layer.visible));
            lObj.set("opacity", json::Value::makeNumber(layer.opacity));
            lObj.set("blend", json::Value::makeNumber((int) layer.blendMode));

            json::Value strokesArr = json::Value::makeArray();
            for (auto& strokePtr : layer.strokes()) {
                Stroke& stroke = *strokePtr;
                json::Value sObj = json::Value::makeObject();
                sObj.set("color", json::Value::makeNumber((double) (int32_t) stroke.color()));

                json::Value bObj = json::Value::makeObject();
                bObj.set("size", json::Value::makeNumber(stroke.brush().baseSizePx));
                bObj.set("minFactor", json::Value::makeNumber(stroke.brush().minSizeFactor));
                bObj.set("opacity", json::Value::makeNumber(stroke.brush().opacity));
                bObj.set("hardness", json::Value::makeNumber(stroke.brush().hardness));
                bObj.set("spacing", json::Value::makeNumber(stroke.brush().spacing));
                bObj.set("pressureSize", json::Value::makeBool(stroke.brush().pressureAffectsSize));
                bObj.set("pressureOpacity", json::Value::makeBool(stroke.brush().pressureAffectsOpacity));
                bObj.set("blend", json::Value::makeNumber((int) stroke.brush().blendMode));
                sObj.set("brush", bObj);

                json::Value ptsArr = json::Value::makeArray();
                for (const auto& p : stroke.points()) {
                    json::Value pObj = json::Value::makeObject();
                    pObj.set("x", json::Value::makeNumber(p.x));
                    pObj.set("y", json::Value::makeNumber(p.y));
                    pObj.set("p", json::Value::makeNumber(p.pressure));
                    ptsArr.push(pObj);
                }
                sObj.set("points", ptsArr);
                strokesArr.push(sObj);
            }
            lObj.set("strokes", strokesArr);
            layersArr.push(lObj);
        }
        fObj.set("layers", layersArr);
        framesArr.push(fObj);
    }
    root.set("frames", framesArr);

    json::Value camObj = json::Value::makeObject();
    json::Value keysArr = json::Value::makeArray();
    for (const auto& k : cameraTrack_.keys()) {
        json::Value kObj = json::Value::makeObject();
        kObj.set("frame", json::Value::makeNumber(k.frameIndex));
        kObj.set("cx", json::Value::makeNumber(k.pose.centerX));
        kObj.set("cy", json::Value::makeNumber(k.pose.centerY));
        kObj.set("zoom", json::Value::makeNumber(k.pose.zoom));
        kObj.set("rot", json::Value::makeNumber(k.pose.rotationDeg));
        kObj.set("easing", json::Value::makeNumber((int) k.easing));
        kObj.set("hold", json::Value::makeBool(k.hold));
        keysArr.push(kObj);
    }
    camObj.set("keys", keysArr);
    camObj.set("aspectPreset", json::Value::makeNumber(cameraAspectPreset_));
    root.set("camera", camObj);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        LOGE("Nao foi possivel abrir '%s' para escrita", path.c_str());
        return false;
    }
    std::string text = root.dump();
    out.write(text.data(), (std::streamsize) text.size());
    bool okWrite = out.good();
    out.close();
    if (!okWrite) LOGE("Falha ao escrever o projeto em '%s'", path.c_str());
    else LOGI("Projeto salvo em '%s' (%zu bytes)", path.c_str(), text.size());
    return okWrite;
}

bool GLRenderEngine::loadProjectFromFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false; // arquivo ainda nao existe (ex.: primeira execucao) - nao eh erro
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string text = ss.str();
    in.close();
    if (text.empty()) return false;

    bool parsedOk = false;
    json::Value root = json::parse(text, &parsedOk);
    if (!parsedOk || root.type() != json::Type::Object) {
        LOGE("Projeto em '%s' invalido ou corrompido", path.c_str());
        return false;
    }

    std::lock_guard<std::mutex> lock(timelineMutex_);

    int framerate = root.has("framerate") ? root.get("framerate").asInt(24) : 24;
    if (framerate < 1) framerate = 24;

    // Antes de descartar a timeline antiga, enfileira as texturas/FBOs de TODAS as
    // camadas de TODOS os frames para a render thread deletar.
    for (size_t fi = 0; fi < timeline_.frameCount(); ++fi) {
        Frame* oldFrame = timeline_.frameAt(fi);
        if (!oldFrame) continue;
        for (auto& oldLayer : oldFrame->layers) queueLayerGLResourcesForDeletion(*oldLayer);
    }
    timeline_.resetEmpty(framerate);

    for (const auto& fVal : root.get("frames").items()) {
        FrameType type = (FrameType) fVal.get("type").asInt(0);
        Frame* f = timeline_.appendFrame(type);
        f->holdDurationTicks = fVal.has("hold") ? fVal.get("hold").asInt(1) : 1;
        if (f->holdDurationTicks < 1) f->holdDurationTicks = 1;
        f->easing = (Easing) fVal.get("easing").asInt((int) Easing::EaseInOut);

        const json::Value& t = fVal.get("transform");
        f->transform.translateX = t.get("tx").asFloat(0.f);
        f->transform.translateY = t.get("ty").asFloat(0.f);
        f->transform.scale = t.get("scale").asFloat(1.f);
        f->transform.rotationDeg = t.get("rot").asFloat(0.f);
        f->transform.opacity = t.get("opacity").asFloat(1.f);

        f->layers.clear(); // descarta a camada padrao criada pelo construtor de Frame
        for (const auto& lVal : fVal.get("layers").items()) {
            auto layer = std::make_unique<Layer>(lVal.get("name").asString("Camada"));
            layer->visible = lVal.has("visible") ? lVal.get("visible").asBool(true) : true;
            layer->opacity = lVal.get("opacity").asFloat(1.f);
            layer->blendMode = (BlendMode) lVal.get("blend").asInt(0);

            for (const auto& sVal : lVal.get("strokes").items()) {
                uint32_t color = (uint32_t) (int32_t) sVal.get("color").asInt(0);
                Brush brush;
                const json::Value& bVal = sVal.get("brush");
                brush.baseSizePx = bVal.get("size").asFloat(24.f);
                brush.minSizeFactor = bVal.get("minFactor").asFloat(0.2f);
                brush.opacity = bVal.get("opacity").asFloat(1.f);
                brush.hardness = bVal.get("hardness").asFloat(0.75f);
                brush.spacing = bVal.get("spacing").asFloat(0.1f);
                brush.pressureAffectsSize = bVal.has("pressureSize") ? bVal.get("pressureSize").asBool(true) : true;
                brush.pressureAffectsOpacity = bVal.has("pressureOpacity") ? bVal.get("pressureOpacity").asBool(true) : true;
                brush.blendMode = (BlendMode) bVal.get("blend").asInt(0);

                auto stroke = std::make_unique<Stroke>(nextStrokeId_++, brush, color);
                for (const auto& pVal : sVal.get("points").items()) {
                    DrawPoint p;
                    p.x = pVal.get("x").asFloat(0.f);
                    p.y = pVal.get("y").asFloat(0.f);
                    p.pressure = pVal.get("p").asFloat(1.f);
                    stroke->addPoint(p);
                }
                stroke->finish();
                layer->addStroke(std::move(stroke)); // marca dirty=true; redesenha no proximo frame
            }
            f->layers.push_back(std::move(layer));
        }
        if (f->layers.empty()) f->layers.push_back(std::make_unique<Layer>("Camada 1"));
    }
    if (timeline_.frameCount() == 0) timeline_.appendFrame(FrameType::Drawn); // projeto vazio: garante ao menos 1 frame

    cameraTrack_ = CameraTrack{};
    if (root.has("camera")) {
        const json::Value& camObj = root.get("camera");
        for (const auto& kVal : camObj.get("keys").items()) {
            CameraKey k;
            k.frameIndex = kVal.get("frame").asInt(0);
            k.pose.centerX = kVal.get("cx").asFloat(0.f);
            k.pose.centerY = kVal.get("cy").asFloat(0.f);
            k.pose.zoom = kVal.get("zoom").asFloat(1.f);
            k.pose.rotationDeg = kVal.get("rot").asFloat(0.f);
            k.easing = (Easing) kVal.get("easing").asInt((int) Easing::EaseInOut);
            k.hold = kVal.has("hold") ? kVal.get("hold").asBool(false) : false;
            cameraTrack_.setKey(k);
        }
        cameraAspectPreset_ = camObj.has("aspectPreset") ? camObj.get("aspectPreset").asInt(0) : 0;
    }

    if (root.has("brush")) {
        const json::Value& bObj = root.get("brush");
        if (bObj.has("color")) currentColorArgb_ = (uint32_t) (int32_t) bObj.get("color").asInt((int32_t) currentColorArgb_);
        if (bObj.has("size")) currentBrush_.baseSizePx = bObj.get("size").asFloat(currentBrush_.baseSizePx);
        if (bObj.has("hardness")) currentBrush_.hardness = bObj.get("hardness").asFloat(currentBrush_.hardness);
    }

    currentFrameIndex_ = root.has("currentFrameIndex") ? root.get("currentFrameIndex").asInt(0) : 0;
    if (currentFrameIndex_ < 0 || currentFrameIndex_ >= (int) timeline_.frameCount()) currentFrameIndex_ = 0;
    activeLayerIndex_ = 0;
    activeStroke_ = nullptr;
    activeLayer_ = nullptr;
    undoRecords_.clear();
    redoRecords_.clear();

    LOGI("Projeto carregado de '%s' (%zu frames)", path.c_str(), timeline_.frameCount());
    return true;
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
    if (numConfigs == 0) { LOGE("Nenhum EGLConfig compativel"); return false; }

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
    presentProgram_ = linkProgram(kCompositeVertexShader, kPresentFragmentShader);
    solidProgram_ = linkProgram(kSolidVertexShader, kSolidFragmentShader);

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
    glGenBuffers(1, &overlayVbo_);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return strokeProgram_ != 0 && compositeProgram_ != 0 && presentProgram_ != 0;
}

void GLRenderEngine::destroyEGL() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) eglDestroyContext(display_, context_);
        if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_, surface_);
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
        surface_ = EGL_NO_SURFACE;
        context_ = EGL_NO_CONTEXT;
    }
    // A referencia ao ANativeWindow e liberada mesmo quando initEGL falhou antes de
    // criar o display (antes isso vazava, pois o return antecipado pulava o release).
    if (window_) {
        ANativeWindow_release(window_);
        window_ = nullptr;
    }
}

void GLRenderEngine::renderLoop() {
    if (!initEGL(window_)) {
        LOGE("Inicializacao EGL falhou - encerrando thread de render");
        destroyEGL();
        running_ = false;
        return;
    }
    resetGLStateForNewContext();
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
                    // Estamos na render thread com o contexto corrente: deleta direto,
                    // em TODOS os frames (nao so no corrente).
                    for (size_t fi = 0; fi < timeline_.frameCount(); ++fi) {
                        Frame* f = timeline_.frameAt(fi);
                        if (!f) continue;
                        for (auto& l : f->layers) {
                            if (l->textureHandle != -1) { GLuint t = (GLuint) l->textureHandle; glDeleteTextures(1, &t); }
                            if (l->fboHandle != -1) { GLuint fb = (GLuint) l->fboHandle; glDeleteFramebuffers(1, &fb); }
                            l->textureHandle = -1;
                            l->fboHandle = -1;
                            l->dirty = true;
                        }
                    }
                    break;
                }
                case RenderCommand::Kind::BeginStroke: {
                    std::lock_guard<std::mutex> lock(timelineMutex_);
                    if (cameraViewMode_) break;
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
                        redoRecords_.clear();
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

void GLRenderEngine::ensureSceneTarget() {
    if (sceneFbo_ != 0 && sceneWidth_ == width_ && sceneHeight_ == height_) return;
    if (sceneFbo_ != 0) {
        glDeleteFramebuffers(1, &sceneFbo_);
        glDeleteTextures(1, &sceneTexture_);
        sceneFbo_ = 0;
        sceneTexture_ = 0;
    }
    if (width_ <= 0 || height_ <= 0) return;

    glGenTextures(1, &sceneTexture_);
    glBindTexture(GL_TEXTURE_2D, sceneTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &sceneFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTexture_, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("FBO da cena incompleto");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    sceneWidth_ = width_;
    sceneHeight_ = height_;
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

void GLRenderEngine::appendThickLine(std::vector<float>& out, float x0, float y0, float x1, float y1, float thickness) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-4f) return;
    float nx = -dy / len * thickness * 0.5f;
    float ny = dx / len * thickness * 0.5f;
    const float v[] = {
        x0 + nx, y0 + ny,  x0 - nx, y0 - ny,  x1 + nx, y1 + ny,
        x1 + nx, y1 + ny,  x0 - nx, y0 - ny,  x1 - nx, y1 - ny,
    };
    out.insert(out.end(), std::begin(v), std::end(v));
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

void GLRenderEngine::presentScene(const CameraPose& pose) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);

    const float W = (float) width_, H = (float) height_;
    float translateX = 0.f, translateY = 0.f, scale = 1.f, rotationRad = 0.f;

    if (cameraViewMode_) {
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        const float aspect = cameraAspectValue();
        const float fw = std::min(W, H * aspect);
        const float fh = fw / aspect;
        const float fit = std::min(W / fw, H / fh);
        const float halfW = fw * fit * 0.5f;
        const float halfH = fh * fit * 0.5f;

        glEnable(GL_SCISSOR_TEST);
        glScissor((GLint) std::floor(W * 0.5f - halfW), (GLint) std::floor(H * 0.5f - halfH),
                  (GLsizei) std::ceil(halfW * 2.f), (GLsizei) std::ceil(halfH * 2.f));
        glClearColor(0.3f, 0.3f, 0.3f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        scale = fit * pose.zoom;
        rotationRad = -pose.rotationDeg * kDegToRad;
        const float c = std::cos(rotationRad), s = std::sin(rotationRad);
        const float vx = W * 0.5f - pose.centerX;
        const float vy = H * 0.5f - pose.centerY;
        translateX = scale * (vx * c - vy * s);
        translateY = scale * (vx * s + vy * c);
    } else {
        glClearColor(0.55f, 0.55f, 0.55f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    glUseProgram(presentProgram_);
    glUniform2f(glGetUniformLocation(presentProgram_, "uViewportSize"), W, H);
    glUniform2f(glGetUniformLocation(presentProgram_, "uTranslate"), translateX, translateY);
    glUniform1f(glGetUniformLocation(presentProgram_, "uScale"), scale);
    glUniform1f(glGetUniformLocation(presentProgram_, "uRotationRad"), rotationRad);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTexture_);
    glUniform1i(glGetUniformLocation(presentProgram_, "uTexture"), 0);

    glBindBuffer(GL_ARRAY_BUFFER, fullscreenQuadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*) (2 * sizeof(float)));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!cameraViewMode_) drawCameraOverlay(pose);
}

void GLRenderEngine::drawCameraOverlay(const CameraPose& pose) {
    if (solidProgram_ == 0) return;

    const float W = (float) width_, H = (float) height_;
    const float aspect = cameraAspectValue();
    const float fw = std::min(W, H * aspect);
    const float fh = fw / aspect;
    const float hx = fw * 0.5f / pose.zoom;
    const float hy = fh * 0.5f / pose.zoom;
    const float rad = pose.rotationDeg * kDegToRad;
    const float c = std::cos(rad), s = std::sin(rad);
    auto toCanvas = [&](float ox, float oy, float& x, float& y) {
        x = pose.centerX + ox * c - oy * s;
        y = pose.centerY + ox * s + oy * c;
    };

    glUseProgram(solidProgram_);
    glUniform2f(glGetUniformLocation(solidProgram_, "uViewportSize"), W, H);
    const GLint colorLoc = glGetUniformLocation(solidProgram_, "uColor");

    glBindBuffer(GL_ARRAY_BUFFER, overlayVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*) 0);
    glDisableVertexAttribArray(1);

    auto flush = [&](float r, float g, float b, float a) {
        if (overlayScratch_.empty()) return;
        glUniform4f(colorLoc, r, g, b, a);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (overlayScratch_.size() * sizeof(float)),
                     overlayScratch_.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei) (overlayScratch_.size() / 2));
        overlayScratch_.clear();
    };
    auto appendSquare = [&](float cx, float cy, float half) {
        const float v[] = {
            cx - half, cy - half,  cx + half, cy - half,  cx - half, cy + half,
            cx - half, cy + half,  cx + half, cy - half,  cx + half, cy + half,
        };
        overlayScratch_.insert(overlayScratch_.end(), std::begin(v), std::end(v));
    };

    overlayScratch_.clear();

    const auto& keys = cameraTrack_.keys();
    if (cameraPathVisible_ && keys.size() >= 2) {
        const int f0 = keys.front().frameIndex;
        const int f1 = keys.back().frameIndex;
        const int step = std::max(1, (f1 - f0) / 1000);
        CameraPose prev = resolveCameraPose(f0);
        for (int f = f0 + step;; f += step) {
            if (f > f1) f = f1;
            CameraPose cur = resolveCameraPose(f);
            appendThickLine(overlayScratch_, prev.centerX, prev.centerY, cur.centerX, cur.centerY, 2.f);
            prev = cur;
            if (f == f1) break;
        }
        flush(1.f, 1.f, 1.f, 0.85f);
        for (const auto& k : keys) appendSquare(k.pose.centerX, k.pose.centerY, 5.f);
        flush(1.f, 0.6f, 0.1f, 1.f);
    }

    float px[4], py[4];
    toCanvas(-hx, -hy, px[0], py[0]);
    toCanvas( hx, -hy, px[1], py[1]);
    toCanvas( hx,  hy, px[2], py[2]);
    toCanvas(-hx,  hy, px[3], py[3]);
    for (int i = 0; i < 4; ++i) {
        int j = (i + 1) % 4;
        appendThickLine(overlayScratch_, px[i], py[i], px[j], py[j], 3.f);
    }
    float tx0, ty0, tx1, ty1;
    toCanvas(0.f, -hy, tx0, ty0);
    toCanvas(0.f, -hy - 16.f, tx1, ty1);
    appendThickLine(overlayScratch_, tx0, ty0, tx1, ty1, 3.f);
    appendThickLine(overlayScratch_, pose.centerX - 12.f, pose.centerY, pose.centerX + 12.f, pose.centerY, 2.f);
    appendThickLine(overlayScratch_, pose.centerX, pose.centerY - 12.f, pose.centerX, pose.centerY + 12.f, 2.f);

    const bool hasKey = cameraTrack_.keyAt(currentFrameIndex_) != nullptr;
    if (hasKey) flush(1.f, 0.6f, 0.1f, 1.f);
    else flush(0.2f, 0.7f, 1.f, 0.95f);
}

void GLRenderEngine::drawFrame() {
    std::lock_guard<std::mutex> lock(timelineMutex_);

    drainPendingGLDeletions();

    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);

    ensureSceneTarget();
    if (sceneFbo_ == 0) return;

    Frame* renderFrame = nullptr;
    Transform transform;
    if (currentFrameIndex_ >= 0 && currentFrameIndex_ < (int) timeline_.frameCount()) {
        renderFrame = timeline_.contentSourceFrame((size_t) currentFrameIndex_);
        transform = timeline_.resolveTransformForFrameIndex((size_t) currentFrameIndex_);
    }

    if (renderFrame) {
        for (auto& layerPtr : renderFrame->layers) {
            Layer& layer = *layerPtr;
            if (!layer.visible) continue;
            ensureLayerTarget(layer);
            if (layer.dirty) {
                renderLayerContents(layer);
                layer.dirty = false;
            }
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.93f, 0.93f, 0.93f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (renderFrame) {
        for (auto& layerPtr : renderFrame->layers) {
            Layer& layer = *layerPtr;
            if (!layer.visible) continue;
            compositeLayer(layer, transform);
        }
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    presentScene(resolveCameraPose(currentFrameIndex_));
}

} // namespace dreams

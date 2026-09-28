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
#include "camera.h"

namespace dreams {

struct RenderCommand {
    enum class Kind { AddPoint, BeginStroke, EndStroke, Resize, Shutdown } kind;
    DrawPoint point;
    float width = 0, height = 0;
};

struct UndoRecord {
    int frameIndex;
    int layerIndex;
};

struct RedoRecord {
    int frameIndex;
    int layerIndex;
    std::unique_ptr<Stroke> stroke;
};

// Motor de renderização OpenGL ES 3. Ver revisões anteriores (FBO por camada, VBO
// persistente, playback, keyframes de objeto, pincel, undo/redo). Esta revisão
// adiciona a CÂMERA: as camadas são compostas numa textura de cena (tamanho do
// canvas) e essa textura é apresentada na tela de duas formas — vista de edição
// (canvas inteiro + moldura da câmera sobreposta) ou vista da câmera (só o que o
// quadro da câmera enxerga, com letterbox).
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

    // --- Câmera ---
    void setCameraViewMode(bool enabled);   // true = preview do que a câmera enxerga (desenho desativado)
    bool isCameraViewMode();
    void setCameraAspectPreset(int preset); // 0=canvas, 1=16:9, 2=4:3, 3=1:1, 4=9:16
    int cameraAspectPreset();
    void setCameraPathVisible(bool visible);
    void setCameraKeyAtCurrentFrame();      // cria chave no frame atual com a pose atual
    void removeCameraKeyAtCurrentFrame();
    bool hasCameraKeyAtCurrentFrame();
    int cameraKeyCount();
    void nudgeCamera(float dx, float dy, float zoomMultiplier, float dRotationDeg); // cria chave se não houver
    void resetCamera(int what);             // 0=tudo, 1=posição, 2=zoom, 3=rotação
    void setCameraEasing(int easing);       // 0=linear, 1=ease in, 2=ease out, 3=ease in-out
    int cameraEasing();
    void setCameraHold(bool hold);
    bool cameraHold();

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

    // --- Keyframes de objeto / autoria de transform ---
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

    // Câmera (os métodos abaixo assumem timelineMutex_ já travado pelo chamador)
    CameraPose defaultCameraPose() const;
    CameraPose resolveCameraPose(int frameIndex) const;
    float cameraAspectValue() const;
    CameraKey& ensureCameraKeyAtCurrentFrame();
    void ensureSceneTarget();
    void presentScene(const CameraPose& pose);
    void drawCameraOverlay(const CameraPose& pose);
    static void appendThickLine(std::vector<float>& out, float x0, float y0, float x1, float y1, float thickness);

    void ensureStrokeVboCapacity(size_t requiredBytes);
    static void appendStrokeQuadVertices(std::vector<float>& out, const Stroke& stroke);

    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    ANativeWindow* window_ = nullptr;

    int width_ = 0, height_ = 0;
    GLuint strokeProgram_ = 0;
    GLuint compositeProgram_ = 0;
    GLuint presentProgram_ = 0;   // desenha a textura de cena na tela (alpha sempre 1)
    GLuint solidProgram_ = 0;     // cor sólida, usado para o overlay da câmera
    GLuint fullscreenQuadVbo_ = 0;
    GLuint overlayVbo_ = 0;
    std::vector<float> overlayScratch_;

    GLuint sceneTexture_ = 0;
    GLuint sceneFbo_ = 0;
    int sceneWidth_ = 0, sceneHeight_ = 0;

    GLuint strokeVbo_ = 0;
    size_t strokeVboCapacityBytes_ = 0;
    std::vector<float> strokeVertexScratch_;

    std::mutex timelineMutex_;
    Timeline timeline_{24};
    int currentFrameIndex_ = 0;
    int activeLayerIndex_ = 0;

    CameraTrack cameraTrack_;
    bool cameraViewMode_ = false;
    bool cameraPathVisible_ = true;
    int cameraAspectPreset_ = 0;

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

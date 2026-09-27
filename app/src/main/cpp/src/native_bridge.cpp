#include <jni.h>
#include <android/native_window_jni.h>
#include <memory>
#include <string>
#include "gl_render_engine.h"

namespace {
std::unique_ptr<dreams::GLRenderEngine> g_engine;
}

extern "C" {

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeSurfaceCreated(
        JNIEnv* env, jobject, jobject surface) {
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!g_engine) g_engine = std::make_unique<dreams::GLRenderEngine>();
    g_engine->start(window);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeSurfaceChanged(
        JNIEnv*, jobject, jint width, jint height) {
    if (g_engine) g_engine->onSurfaceResized(width, height);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeSurfaceDestroyed(
        JNIEnv*, jobject) {
    if (g_engine) g_engine->stop();
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeTouchDown(
        JNIEnv*, jobject, jfloat x, jfloat y, jfloat pressure) {
    if (g_engine) g_engine->onTouchDown(x, y, pressure);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeTouchMove(
        JNIEnv*, jobject, jfloat x, jfloat y, jfloat pressure) {
    if (g_engine) g_engine->onTouchMove(x, y, pressure);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeTouchUp(
        JNIEnv*, jobject) {
    if (g_engine) g_engine->onTouchUp();
}

// --- Câmera ---

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativePanCamera(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    if (g_engine) g_engine->panCamera(dx, dy);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeZoomCamera(
        JNIEnv*, jobject, jfloat factor, jfloat pivotX, jfloat pivotY) {
    if (g_engine) g_engine->zoomCamera(factor, pivotX, pivotY);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeRotateCamera(JNIEnv*, jobject, jfloat deltaDeg) {
    if (g_engine) g_engine->rotateCamera(deltaDeg);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeResetCamera(JNIEnv*, jobject) {
    if (g_engine) g_engine->resetCamera();
}

// --- Pincel atual ---

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeSetBrushColor(JNIEnv*, jobject, jint argb) {
    if (g_engine) g_engine->setBrushColor((uint32_t) argb);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeSetBrushSize(JNIEnv*, jobject, jfloat px) {
    if (g_engine) g_engine->setBrushSize(px);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeSetBrushHardness(JNIEnv*, jobject, jfloat hardness) {
    if (g_engine) g_engine->setBrushHardness(hardness);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeSetEraserMode(JNIEnv*, jobject, jboolean enabled) {
    if (g_engine) g_engine->setEraserMode(enabled == JNI_TRUE);
}

JNIEXPORT jboolean JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeIsEraserMode(JNIEnv*, jobject) {
    return g_engine ? (jboolean) g_engine->isEraserMode() : JNI_FALSE;
}

// --- Undo / redo ---

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeUndo(JNIEnv*, jobject) {
    if (g_engine) g_engine->undo();
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeRedo(JNIEnv*, jobject) {
    if (g_engine) g_engine->redo();
}

JNIEXPORT jboolean JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeCanUndo(JNIEnv*, jobject) {
    return g_engine ? (jboolean) g_engine->canUndo() : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeCanRedo(JNIEnv*, jobject) {
    return g_engine ? (jboolean) g_engine->canRedo() : JNI_FALSE;
}

// --- Camadas ---

JNIEXPORT jint JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeGetLayerCount(JNIEnv*, jobject) {
    return g_engine ? g_engine->layerCount() : 0;
}

JNIEXPORT jstring JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeGetLayerName(JNIEnv* env, jobject, jint index) {
    std::string name = g_engine ? g_engine->layerName(index) : "";
    return env->NewStringUTF(name.c_str());
}

JNIEXPORT jboolean JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeGetLayerVisible(JNIEnv*, jobject, jint index) {
    return g_engine ? (jboolean) g_engine->layerVisible(index) : JNI_FALSE;
}

JNIEXPORT jfloat JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeGetLayerOpacity(JNIEnv*, jobject, jint index) {
    return g_engine ? g_engine->layerOpacity(index) : 1.f;
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeSetLayerVisible(JNIEnv*, jobject, jint index, jboolean visible) {
    if (g_engine) g_engine->setLayerVisible(index, visible == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeSetLayerOpacity(JNIEnv*, jobject, jint index, jfloat opacity) {
    if (g_engine) g_engine->setLayerOpacity(index, opacity);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeSetActiveLayer(JNIEnv*, jobject, jint index) {
    if (g_engine) g_engine->setActiveLayer(index);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeAddLayer(JNIEnv* env, jobject, jstring name) {
    if (!g_engine) return;
    const char* chars = env->GetStringUTFChars(name, nullptr);
    g_engine->addLayer(chars ? chars : "Camada");
    if (chars) env->ReleaseStringUTFChars(name, chars);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeRemoveLayer(JNIEnv*, jobject, jint index) {
    if (g_engine) g_engine->removeLayer(index);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeMoveLayer(JNIEnv*, jobject, jint from, jint to) {
    if (g_engine) g_engine->moveLayer(from, to);
}

// --- Timeline ---

JNIEXPORT jint JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeGetFrameCount(JNIEnv*, jobject) {
    return g_engine ? g_engine->frameCount() : 0;
}

JNIEXPORT jint JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeGetCurrentFrameIndex(JNIEnv*, jobject) {
    return g_engine ? g_engine->currentFrameIndex() : 0;
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeAddFrame(JNIEnv*, jobject) {
    if (g_engine) g_engine->addFrame();
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeGoToFrame(JNIEnv*, jobject, jint index) {
    if (g_engine) g_engine->goToFrame(index);
}

// --- Keyframes / autoria de transform ---

JNIEXPORT jint JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeGetFrameType(JNIEnv*, jobject, jint index) {
    return g_engine ? g_engine->frameType(index) : 0;
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeSetFrameType(JNIEnv*, jobject, jint index, jint type) {
    if (g_engine) g_engine->setFrameType(index, type);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeNudgeFrameTransform(
        JNIEnv*, jobject, jint index, jfloat dTx, jfloat dTy, jfloat dScale, jfloat dRotationDeg) {
    if (g_engine) g_engine->nudgeFrameTransform(index, dTx, dTy, dScale, dRotationDeg);
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeAppendInterpolatedFrame(JNIEnv*, jobject) {
    if (g_engine) g_engine->appendInterpolatedFrame();
}

// --- Playback ---

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativePlay(JNIEnv*, jobject) {
    if (g_engine) g_engine->play();
}

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativePause(JNIEnv*, jobject) {
    if (g_engine) g_engine->pause();
}

JNIEXPORT jboolean JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeIsPlaying(JNIEnv*, jobject) {
    return g_engine ? (jboolean) g_engine->isPlaying() : JNI_FALSE;
}

// --- Projeto ---

JNIEXPORT jboolean JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeSaveProject(JNIEnv* env, jobject, jstring path) {
    if (!g_engine) return JNI_FALSE;
    const char* chars = env->GetStringUTFChars(path, nullptr);
    bool ok = g_engine->saveProject(chars ? chars : "");
    if (chars) env->ReleaseStringUTFChars(path, chars);
    return (jboolean) ok;
}

JNIEXPORT jboolean JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeLoadProject(JNIEnv* env, jobject, jstring path) {
    if (!g_engine) return JNI_FALSE;
    const char* chars = env->GetStringUTFChars(path, nullptr);
    bool ok = g_engine->loadProject(chars ? chars : "");
    if (chars) env->ReleaseStringUTFChars(path, chars);
    return (jboolean) ok;
}

} // extern "C"

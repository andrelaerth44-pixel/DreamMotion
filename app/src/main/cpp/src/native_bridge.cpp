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

} // extern "C"

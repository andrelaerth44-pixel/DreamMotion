#include <jni.h>
#include <android/native_window_jni.h>
#include <memory>
#include "gl_render_engine.h"

namespace {
std::unique_ptr<dreams::GLRenderEngine> g_engine;
}

extern "C" {

// Repassa o ciclo de vida da Surface (SurfaceHolder.Callback do lado Kotlin) e os
// eventos de toque para a engine nativa. Mantido deliberadamente fino: nenhuma
// lógica de desenho vive aqui, só a travessia JNI.

JNIEXPORT void JNICALL
Java_com_tracostudio_dreams_engine_NativeEngine_nativeSurfaceCreated(
        JNIEnv* env, jobject /*thiz*/, jobject surface) {
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!g_engine) g_engine = std::make_unique<dreams::GLRenderEngine>();
    // A engine assume a posse dessa referência do ANativeWindow e a libera em
    // destroyEGL() quando a thread de render encerra — não liberar aqui.
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

} // extern "C"

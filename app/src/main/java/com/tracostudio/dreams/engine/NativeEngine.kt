package com.tracostudio.dreams.engine

import android.view.Surface

/**
 * Ponte fina para a engine nativa em C++ (libdreamsengine.so). Cada método aqui
 * corresponde a uma função JNI implementada em app/src/main/cpp/src/native_bridge.cpp.
 * Sem estado do lado Kotlin de propósito — todo o estado (traços, camadas, timeline,
 * contexto EGL) vive no C++ para minimizar cruzamentos JNI por frame.
 */
object NativeEngine {
    init {
        System.loadLibrary("dreamsengine")
    }

    external fun nativeSurfaceCreated(surface: Surface)
    external fun nativeSurfaceChanged(width: Int, height: Int)
    external fun nativeSurfaceDestroyed()

    external fun nativeTouchDown(x: Float, y: Float, pressure: Float)
    external fun nativeTouchMove(x: Float, y: Float, pressure: Float)
    external fun nativeTouchUp()
}

package com.tracostudio.dreams.engine

import android.view.Surface

/**
 * Ponte fina para a engine nativa em C++ (libdreamsengine.so). Cada método aqui
 * corresponde a uma função JNI implementada em app/src/main/cpp/src/native_bridge.cpp.
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

    // --- Camadas do frame corrente ---
    external fun nativeGetLayerCount(): Int
    external fun nativeGetLayerName(index: Int): String
    external fun nativeGetLayerVisible(index: Int): Boolean
    external fun nativeGetLayerOpacity(index: Int): Float
    external fun nativeSetLayerVisible(index: Int, visible: Boolean)
    external fun nativeSetLayerOpacity(index: Int, opacity: Float)
    external fun nativeSetActiveLayer(index: Int)
    external fun nativeAddLayer(name: String)
    external fun nativeRemoveLayer(index: Int)
    external fun nativeMoveLayer(from: Int, to: Int)

    // --- Timeline ---
    external fun nativeGetFrameCount(): Int
    external fun nativeGetCurrentFrameIndex(): Int
    external fun nativeAddFrame()
    external fun nativeGoToFrame(index: Int)

    // --- Keyframes / autoria de transform. Tipos: 0=Drawn, 1=Keyframe, 2=Interpolated ---
    external fun nativeGetFrameType(index: Int): Int
    external fun nativeSetFrameType(index: Int, type: Int)
    external fun nativeNudgeFrameTransform(index: Int, dTx: Float, dTy: Float, dScale: Float, dRotationDeg: Float)
    external fun nativeAppendInterpolatedFrame()

    // --- Playback ---
    external fun nativePlay()
    external fun nativePause()
    external fun nativeIsPlaying(): Boolean
}

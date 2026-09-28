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

    // --- Pincel atual ---
    external fun nativeSetBrushColor(argb: Int)
    external fun nativeSetBrushSize(px: Float)
    external fun nativeSetBrushHardness(hardness: Float)
    external fun nativeSetEraserMode(enabled: Boolean)
    external fun nativeIsEraserMode(): Boolean

    // --- Undo / redo (em nível de traço) ---
    external fun nativeUndo()
    external fun nativeRedo()
    external fun nativeCanUndo(): Boolean
    external fun nativeCanRedo(): Boolean

    // --- Câmera. Aspect: 0=canvas, 1=16:9, 2=4:3, 3=1:1, 4=9:16.
    //     Easing: 0=linear, 1=ease in, 2=ease out, 3=ease in-out.
    //     Reset: 0=tudo, 1=posição, 2=zoom, 3=rotação. ---
    external fun nativeSetCameraViewMode(enabled: Boolean)
    external fun nativeIsCameraViewMode(): Boolean
    external fun nativeSetCameraAspectPreset(preset: Int)
    external fun nativeGetCameraAspectPreset(): Int
    external fun nativeSetCameraPathVisible(visible: Boolean)
    external fun nativeSetCameraKey()
    external fun nativeRemoveCameraKey()
    external fun nativeHasCameraKey(): Boolean
    external fun nativeGetCameraKeyCount(): Int
    external fun nativeNudgeCamera(dx: Float, dy: Float, zoomMultiplier: Float, dRotationDeg: Float)
    external fun nativeResetCamera(what: Int)
    external fun nativeSetCameraEasing(easing: Int)
    external fun nativeGetCameraEasing(): Int
    external fun nativeSetCameraHold(hold: Boolean)
    external fun nativeGetCameraHold(): Boolean

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

    // --- Keyframes de objeto. Tipos: 0=Drawn, 1=Keyframe, 2=Interpolated ---
    external fun nativeGetFrameType(index: Int): Int
    external fun nativeSetFrameType(index: Int, type: Int)
    external fun nativeNudgeFrameTransform(index: Int, dTx: Float, dTy: Float, dScale: Float, dRotationDeg: Float)
    external fun nativeAppendInterpolatedFrame()

    // --- Playback ---
    external fun nativePlay()
    external fun nativePause()
    external fun nativeIsPlaying(): Boolean
}

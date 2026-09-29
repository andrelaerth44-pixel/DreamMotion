package com.tracostudio.dreams.canvas

import android.content.Context
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import com.tracostudio.dreams.engine.NativeEngine
import java.io.File

/**
 * SurfaceView "burro" do lado Kotlin: só repassa o ciclo de vida da superfície e os
 * eventos de toque para a engine nativa, que roda seu próprio render loop em uma
 * thread C++ dedicada (ver GLRenderEngine::renderLoop).
 *
 * Também dispara o autoload do projeto assim que a engine nativa existe (logo
 * após nativeSurfaceCreated) — antes disso, g_engine ainda é nulo do lado C++ e
 * a chamada seria um no-op silencioso.
 */
class DreamsSurfaceView(context: Context, attrs: AttributeSet? = null) :
    SurfaceView(context, attrs), SurfaceHolder.Callback {

    init {
        holder.addCallback(this)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        NativeEngine.nativeSurfaceCreated(holder.surface)
        val path = File(context.filesDir, "current_project.json").absolutePath
        NativeEngine.nativeLoadProject(path) // no-op silencioso se o arquivo ainda nao existir
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        NativeEngine.nativeSurfaceChanged(width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        NativeEngine.nativeSurfaceDestroyed()
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val pressure = event.pressure.coerceIn(0f, 1f)
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN ->
                NativeEngine.nativeTouchDown(event.x, event.y, pressure)
            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until event.historySize) {
                    NativeEngine.nativeTouchMove(
                        event.getHistoricalX(i), event.getHistoricalY(i),
                        event.getHistoricalPressure(i).coerceIn(0f, 1f)
                    )
                }
                NativeEngine.nativeTouchMove(event.x, event.y, pressure)
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL ->
                NativeEngine.nativeTouchUp()
        }
        return true
    }
}

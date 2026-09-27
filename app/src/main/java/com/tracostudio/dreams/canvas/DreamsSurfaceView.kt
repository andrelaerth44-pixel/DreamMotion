package com.tracostudio.dreams.canvas

import android.content.Context
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import com.tracostudio.dreams.engine.NativeEngine
import kotlin.math.atan2
import kotlin.math.hypot

/**
 * SurfaceView do canvas. Um dedo desenha; dois dedos viram gesto de câmera
 * (pan + pinch-zoom + rotação), tudo resolvido aqui e repassado à engine
 * nativa via NativeEngine.native{Pan,Zoom,Rotate}Camera. Ao entrar em modo de
 * dois dedos, um traço em andamento com o primeiro dedo é cancelado (nativeTouchUp)
 * para não misturar desenho com navegação de canvas.
 */
class DreamsSurfaceView(context: Context, attrs: AttributeSet? = null) :
    SurfaceView(context, attrs), SurfaceHolder.Callback {

    private var isDrawingStroke = false
    private var twoFingerActive = false
    private var lastMidX = 0f
    private var lastMidY = 0f
    private var lastDist = 0f
    private var lastAngleRad = 0f

    init {
        holder.addCallback(this)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        NativeEngine.nativeSurfaceCreated(holder.surface)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        NativeEngine.nativeSurfaceChanged(width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        NativeEngine.nativeSurfaceDestroyed()
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                twoFingerActive = false
                isDrawingStroke = true
                NativeEngine.nativeTouchDown(event.x, event.y, event.pressure.coerceIn(0f, 1f))
            }
            MotionEvent.ACTION_POINTER_DOWN -> {
                if (event.pointerCount == 2) {
                    if (isDrawingStroke) {
                        NativeEngine.nativeTouchUp()
                        isDrawingStroke = false
                    }
                    twoFingerActive = true
                    updateTwoFingerBaseline(event)
                }
            }
            MotionEvent.ACTION_MOVE -> {
                if (twoFingerActive && event.pointerCount >= 2) {
                    handleTwoFingerMove(event)
                } else if (!twoFingerActive) {
                    for (i in 0 until event.historySize) {
                        NativeEngine.nativeTouchMove(
                            event.getHistoricalX(i), event.getHistoricalY(i),
                            event.getHistoricalPressure(i).coerceIn(0f, 1f)
                        )
                    }
                    NativeEngine.nativeTouchMove(event.x, event.y, event.pressure.coerceIn(0f, 1f))
                }
            }
            MotionEvent.ACTION_POINTER_UP -> {
                if (event.pointerCount - 1 < 2) {
                    twoFingerActive = false
                } else {
                    updateTwoFingerBaseline(event)
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (isDrawingStroke) {
                    NativeEngine.nativeTouchUp()
                    isDrawingStroke = false
                }
                twoFingerActive = false
            }
        }
        return true
    }

    private fun updateTwoFingerBaseline(event: MotionEvent) {
        val x0 = event.getX(0); val y0 = event.getY(0)
        val x1 = event.getX(1); val y1 = event.getY(1)
        lastMidX = (x0 + x1) / 2f
        lastMidY = (y0 + y1) / 2f
        lastDist = hypot((x1 - x0).toDouble(), (y1 - y0).toDouble()).toFloat()
        lastAngleRad = atan2((y1 - y0).toDouble(), (x1 - x0).toDouble()).toFloat()
    }

    private fun handleTwoFingerMove(event: MotionEvent) {
        val x0 = event.getX(0); val y0 = event.getY(0)
        val x1 = event.getX(1); val y1 = event.getY(1)
        val midX = (x0 + x1) / 2f
        val midY = (y0 + y1) / 2f
        val dist = hypot((x1 - x0).toDouble(), (y1 - y0).toDouble()).toFloat()
        val angleRad = atan2((y1 - y0).toDouble(), (x1 - x0).toDouble()).toFloat()

        val dx = midX - lastMidX
        val dy = midY - lastMidY
        val zoomFactor = if (lastDist > 1f) dist / lastDist else 1f
        val rotationDeltaDeg = Math.toDegrees((angleRad - lastAngleRad).toDouble()).toFloat()

        NativeEngine.nativePanCamera(dx, dy)
        NativeEngine.nativeZoomCamera(zoomFactor, midX, midY)
        NativeEngine.nativeRotateCamera(rotationDeltaDeg)

        lastMidX = midX; lastMidY = midY; lastDist = dist; lastAngleRad = angleRad
    }
}

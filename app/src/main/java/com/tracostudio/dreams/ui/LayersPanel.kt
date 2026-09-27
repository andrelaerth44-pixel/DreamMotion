package com.tracostudio.dreams.ui

import android.content.Context
import android.graphics.Color
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.CheckBox
import android.widget.LinearLayout
import android.widget.TextView
import com.tracostudio.dreams.engine.NativeEngine

/**
 * Painel utilitário que expõe camadas, navegação/playback de timeline, e agora
 * também a autoria básica de keyframes (marcar um frame como Keyframe e ajustar
 * seu Transform com botões de nudge). Deliberadamente cru (Views programáticas).
 *
 * Fluxo para testar interpolação de verdade:
 * 1. Desenhe algo no frame 1, toque "Marcar Keyframe".
 * 2. Toque "+ Interpolado" (cria um frame vazio do tipo Interpolated).
 * 3. Toque "+ Frame", desenhe... na verdade o próximo Keyframe precisa ser
 *    marcado manualmente após criado — crie um frame, toque "Marcar Keyframe"
 *    de novo, e ajuste a posição/escala/rotação dele com os botões de nudge.
 * 4. Dê Play: o frame Interpolated no meio vai mostrar o desenho do primeiro
 *    keyframe se movendo/escalando/girando em direção à pose do segundo.
 */
class LayersPanel(context: Context) : LinearLayout(context) {

    private val frameLabel = TextView(context)
    private val keyframeToggleButton = Button(context)
    private val layerListContainer = LinearLayout(context).apply { orientation = VERTICAL }

    private val handler = Handler(Looper.getMainLooper())
    private val playbackTick = object : Runnable {
        override fun run() {
            updateFrameLabel()
            if (NativeEngine.nativeIsPlaying()) handler.postDelayed(this, 100)
        }
    }

    init {
        orientation = VERTICAL
        setBackgroundColor(Color.parseColor("#E8E8E8"))
        setPadding(16, 16, 16, 16)

        addView(buildFrameRow())
        addView(buildKeyframeRow())
        addView(layerListContainer)
        addView(buildAddLayerButton())

        refresh()
    }

    private fun buildFrameRow(): View {
        val row = LinearLayout(context).apply {
            orientation = HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        val prev = Button(context).apply {
            text = "◀"
            setOnClickListener {
                val cur = NativeEngine.nativeGetCurrentFrameIndex()
                if (cur > 0) { NativeEngine.nativeGoToFrame(cur - 1); refresh() }
            }
        }
        val next = Button(context).apply {
            text = "▶"
            setOnClickListener {
                val cur = NativeEngine.nativeGetCurrentFrameIndex()
                if (cur < NativeEngine.nativeGetFrameCount() - 1) { NativeEngine.nativeGoToFrame(cur + 1); refresh() }
            }
        }
        val addFrame = Button(context).apply {
            text = "+ Frame"
            setOnClickListener { NativeEngine.nativeAddFrame(); refresh() }
        }
        val playButton = Button(context).apply {
            text = "▶ Play"
            setOnClickListener {
                if (NativeEngine.nativeIsPlaying()) {
                    NativeEngine.nativePause()
                    text = "▶ Play"
                    handler.removeCallbacks(playbackTick)
                    refresh()
                } else {
                    NativeEngine.nativePlay()
                    text = "⏸ Pause"
                    handler.post(playbackTick)
                }
            }
        }
        row.addView(prev)
        row.addView(frameLabel.apply { setPadding(24, 0, 24, 0) })
        row.addView(next)
        row.addView(addFrame)
        row.addView(playButton)
        return row
    }

    private fun buildKeyframeRow(): View {
        val row = LinearLayout(context).apply {
            orientation = HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        keyframeToggleButton.setOnClickListener {
            val idx = NativeEngine.nativeGetCurrentFrameIndex()
            val isKey = NativeEngine.nativeGetFrameType(idx) == 1
            NativeEngine.nativeSetFrameType(idx, if (isKey) 0 else 1)
            refresh()
        }
        val left = Button(context).apply { text = "◄"; setOnClickListener { nudge(-20f, 0f, 0f, 0f) } }
        val right = Button(context).apply { text = "►"; setOnClickListener { nudge(20f, 0f, 0f, 0f) } }
        val up = Button(context).apply { text = "▲"; setOnClickListener { nudge(0f, -20f, 0f, 0f) } }
        val down = Button(context).apply { text = "▼"; setOnClickListener { nudge(0f, 20f, 0f, 0f) } }
        val scaleDown = Button(context).apply { text = "−"; setOnClickListener { nudge(0f, 0f, -0.1f, 0f) } }
        val scaleUp = Button(context).apply { text = "+"; setOnClickListener { nudge(0f, 0f, 0.1f, 0f) } }
        val rotCcw = Button(context).apply { text = "↺"; setOnClickListener { nudge(0f, 0f, 0f, -10f) } }
        val rotCw = Button(context).apply { text = "↻"; setOnClickListener { nudge(0f, 0f, 0f, 10f) } }
        val addInterpolated = Button(context).apply {
            text = "+ Interpolado"
            setOnClickListener { NativeEngine.nativeAppendInterpolatedFrame(); refresh() }
        }
        row.addView(keyframeToggleButton)
        row.addView(left); row.addView(right); row.addView(up); row.addView(down)
        row.addView(scaleDown); row.addView(scaleUp)
        row.addView(rotCcw); row.addView(rotCw)
        row.addView(addInterpolated)
        return row
    }

    private fun nudge(dTx: Float, dTy: Float, dScale: Float, dRot: Float) {
        val idx = NativeEngine.nativeGetCurrentFrameIndex()
        NativeEngine.nativeNudgeFrameTransform(idx, dTx, dTy, dScale, dRot)
    }

    private fun buildAddLayerButton(): View =
        Button(context).apply {
            text = "+ Camada"
            setOnClickListener {
                NativeEngine.nativeAddLayer("Camada ${NativeEngine.nativeGetLayerCount() + 1}")
                refresh()
            }
        }

    private fun updateFrameLabel() {
        val frameIdx = NativeEngine.nativeGetCurrentFrameIndex()
        val frameCount = NativeEngine.nativeGetFrameCount()
        frameLabel.text = "Frame ${frameIdx + 1}/$frameCount"

        keyframeToggleButton.text = when (NativeEngine.nativeGetFrameType(frameIdx)) {
            1 -> "★ Keyframe"
            2 -> "◈ Interpolado"
            else -> "☆ Marcar Keyframe"
        }
    }

    fun refresh() {
        updateFrameLabel()

        layerListContainer.removeAllViews()
        val count = NativeEngine.nativeGetLayerCount()
        for (i in count - 1 downTo 0) {
            layerListContainer.addView(buildLayerRow(i))
        }
    }

    private fun buildLayerRow(index: Int): View {
        val row = LinearLayout(context).apply {
            orientation = HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }

        val visible = CheckBox(context).apply {
            isChecked = NativeEngine.nativeGetLayerVisible(index)
            setOnCheckedChangeListener { _, checked -> NativeEngine.nativeSetLayerVisible(index, checked) }
        }
        val name = TextView(context).apply {
            text = NativeEngine.nativeGetLayerName(index)
            setPadding(8, 0, 8, 0)
            setOnClickListener { NativeEngine.nativeSetActiveLayer(index); refresh() }
        }
        val up = Button(context).apply {
            text = "↑"
            setOnClickListener { NativeEngine.nativeMoveLayer(index, index + 1); refresh() }
        }
        val down = Button(context).apply {
            text = "↓"
            setOnClickListener { NativeEngine.nativeMoveLayer(index, index - 1); refresh() }
        }
        val remove = Button(context).apply {
            text = "✕"
            setOnClickListener { NativeEngine.nativeRemoveLayer(index); refresh() }
        }

        row.addView(visible)
        row.addView(name)
        row.addView(up)
        row.addView(down)
        row.addView(remove)
        return row
    }
}

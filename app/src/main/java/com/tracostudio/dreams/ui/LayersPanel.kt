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
 * Painel utilitário que expõe as camadas, a navegação de frames e o playback da
 * timeline que já funcionam na engine nativa. Deliberadamente cru (Views
 * programáticas, sem estilo) — o objetivo aqui é expor a funcionalidade que a
 * engine já tem, não desenhar a UI final do produto.
 *
 * Durante o playback, quem avança os frames é a própria render thread nativa
 * (ver GLRenderEngine::advancePlayback) — este painel só faz polling leve
 * (a cada 100ms) pra manter o rótulo "Frame X/Y" sincronizado enquanto toca.
 *
 * NOTA: desenhar enquanto o playback está ativo não é bloqueado hoje — um
 * toque durante o play adiciona um traço a qualquer frame que estiver passando
 * naquele instante. Isso é um comportamento a refinar (ex.: pausar
 * automaticamente ao detectar um toque no canvas).
 */
class LayersPanel(context: Context) : LinearLayout(context) {

    private val frameLabel = TextView(context)
    private val layerListContainer = LinearLayout(context).apply { orientation = VERTICAL }
    private val playButton = Button(context)

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
        playButton.apply {
            text = "▶ Play"
            setOnClickListener {
                if (NativeEngine.nativeIsPlaying()) {
                    NativeEngine.nativePause()
                    text = "▶ Play"
                    handler.removeCallbacks(playbackTick)
                    refresh() // playback pode ter mudado a camada/frame corrente
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
    }

    /** Reconstrói rótulo de frame + lista de camadas inteira. Chamar após qualquer ação estrutural. */
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

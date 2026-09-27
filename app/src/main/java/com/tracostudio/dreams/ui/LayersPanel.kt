package com.tracostudio.dreams.ui

import android.content.Context
import android.graphics.Color
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.CheckBox
import android.widget.LinearLayout
import android.widget.TextView
import com.tracostudio.dreams.engine.NativeEngine

/**
 * Painel utilitário que expõe as camadas e a navegação de frames que já funcionam
 * na engine nativa. Deliberadamente cru (Views programáticas, sem estilo) — o
 * objetivo aqui é expor a funcionalidade que a engine já tem, não desenhar a UI
 * final do produto (isso vem depois, com direção visual de verdade).
 *
 * Não há nenhum mecanismo de observação entre a engine nativa e este painel:
 * cada botão chama a JNI diretamente e depois pede refresh() a si mesmo. Isso
 * cobre o caso de uso atual (a estrutura de camadas/frames só muda por ação do
 * usuário neste painel), mas deixa de refletir mudanças feitas por outro caminho.
 */
class LayersPanel(context: Context) : LinearLayout(context) {

    private val frameLabel = TextView(context)
    private val layerListContainer = LinearLayout(context).apply { orientation = VERTICAL }

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
        row.addView(prev)
        row.addView(frameLabel.apply { setPadding(24, 0, 24, 0) })
        row.addView(next)
        row.addView(addFrame)
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

    fun refresh() {
        val frameIdx = NativeEngine.nativeGetCurrentFrameIndex()
        val frameCount = NativeEngine.nativeGetFrameCount()
        frameLabel.text = "Frame ${frameIdx + 1}/$frameCount"

        layerListContainer.removeAllViews()
        val count = NativeEngine.nativeGetLayerCount()
        // Mostra do topo (última camada = mais acima na composição) para a base,
        // como na maioria dos apps de desenho.
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

package com.tracostudio.dreams.ui

import android.content.Context
import android.graphics.Color
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.TextView
import com.tracostudio.dreams.engine.NativeEngine

/**
 * Painel utilitário de pincel: paleta de cores fixas, tamanho e dureza (SeekBar),
 * borracha, e agora undo/redo (em nível de traço completo, não por ponto). Sem
 * seletor de cor livre (roda HSV) ainda.
 */
class BrushPanel(context: Context) : LinearLayout(context) {

    private val eraserButton = Button(context)
    private val undoButton = Button(context)
    private val redoButton = Button(context)

    private val palette = listOf(
        Color.parseColor("#202020"),
        Color.parseColor("#FFFFFF"),
        Color.parseColor("#E53935"),
        Color.parseColor("#1E88E5"),
        Color.parseColor("#43A047"),
        Color.parseColor("#FDD835"),
        Color.parseColor("#8E24AA"),
    )

    init {
        orientation = VERTICAL
        setBackgroundColor(Color.parseColor("#D8D8D8"))
        setPadding(16, 8, 16, 8)

        addView(buildColorRow())
        addView(buildSizeRow())
        addView(buildHardnessRow())

        NativeEngine.nativeSetBrushColor(palette[0])
    }

    private fun buildColorRow(): View {
        val row = LinearLayout(context).apply {
            orientation = HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        for (color in palette) {
            val swatch = Button(context).apply {
                setBackgroundColor(color)
                text = ""
                layoutParams = LinearLayout.LayoutParams(60, 60).apply { setMargins(4, 4, 4, 4) }
                setOnClickListener {
                    NativeEngine.nativeSetBrushColor(color)
                    if (NativeEngine.nativeIsEraserMode()) {
                        NativeEngine.nativeSetEraserMode(false)
                        eraserButton.text = "Borracha"
                    }
                }
            }
            row.addView(swatch)
        }
        eraserButton.apply {
            text = "Borracha"
            setOnClickListener {
                val nowEraser = !NativeEngine.nativeIsEraserMode()
                NativeEngine.nativeSetEraserMode(nowEraser)
                text = if (nowEraser) "✓ Borracha" else "Borracha"
            }
        }
        undoButton.apply {
            text = "↶ Undo"
            setOnClickListener { NativeEngine.nativeUndo() }
        }
        redoButton.apply {
            text = "↷ Redo"
            setOnClickListener { NativeEngine.nativeRedo() }
        }
        row.addView(eraserButton)
        row.addView(undoButton)
        row.addView(redoButton)
        return row
    }

    private fun buildSizeRow(): View {
        val row = LinearLayout(context).apply {
            orientation = HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        val label = TextView(context).apply { text = "Tamanho" }
        val seek = SeekBar(context).apply {
            max = 100
            progress = 24
            layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                    NativeEngine.nativeSetBrushSize((progress + 1).toFloat())
                }
                override fun onStartTrackingTouch(seekBar: SeekBar?) {}
                override fun onStopTrackingTouch(seekBar: SeekBar?) {}
            })
        }
        row.addView(label)
        row.addView(seek)
        return row
    }

    private fun buildHardnessRow(): View {
        val row = LinearLayout(context).apply {
            orientation = HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        val label = TextView(context).apply { text = "Dureza" }
        val seek = SeekBar(context).apply {
            max = 100
            progress = 75
            layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                    NativeEngine.nativeSetBrushHardness(progress / 100f)
                }
                override fun onStartTrackingTouch(seekBar: SeekBar?) {}
                override fun onStopTrackingTouch(seekBar: SeekBar?) {}
            })
        }
        row.addView(label)
        row.addView(seek)
        return row
    }
}

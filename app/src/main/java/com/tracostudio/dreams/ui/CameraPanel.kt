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
 * Painel da câmera. Modelo (inspirado em Pencil2D 0.7, OpenToonz e Procreate Dreams):
 *  - A câmera é um objeto de cena com chaves por frame (posição, zoom, rotação),
 *    separado da vista de edição: mover a câmera não muda o que você enxerga ao editar;
 *    a moldura dela aparece por cima do canvas.
 *  - "Vista da câmera" mostra só o que o quadro da câmera enxerga (desenho desativado).
 *  - Qualquer ajuste (mover/zoom/girar/reset/easing/hold) cria uma chave no frame
 *    atual automaticamente, partindo da pose já resolvida naquele frame.
 *  - Easing por chave e Hold (sem tween) para cortes secos de câmera.
 *
 * Este painel não recebe callbacks da engine: faz polling leve (200ms) enquanto
 * está na tela, para refletir a mudança de frame (navegação ou playback).
 */
class CameraPanel(context: Context) : LinearLayout(context) {

    private val aspectNames = listOf("Canvas", "16:9", "4:3", "1:1", "9:16")
    private val easingNames = listOf("Linear", "Ease In", "Ease Out", "Ease In-Out")

    private val viewToggle = Button(context)
    private val aspectButton = Button(context)
    private val keyButton = Button(context)
    private val easingButton = Button(context)
    private val holdCheck = CheckBox(context)
    private val pathCheck = CheckBox(context)
    private val statusLabel = TextView(context)

    private var updating = false
    private val handler = Handler(Looper.getMainLooper())
    private val poller = object : Runnable {
        override fun run() {
            refresh()
            handler.postDelayed(this, 200)
        }
    }

    init {
        orientation = VERTICAL
        setBackgroundColor(Color.parseColor("#CFCFCF"))
        setPadding(16, 12, 16, 12)

        addView(TextView(context).apply { text = "Câmera"; textSize = 16f })
        addView(viewToggle)
        addView(row(aspectButton, keyButton))
        addView(statusLabel)

        addView(row(
            nudge("◄", -20f, 0f, 1f, 0f), nudge("▲", 0f, -20f, 1f, 0f),
            nudge("▼", 0f, 20f, 1f, 0f), nudge("►", 20f, 0f, 1f, 0f)
        ))
        addView(row(
            nudge("Zoom −", 0f, 0f, 1f / 1.1f, 0f), nudge("Zoom +", 0f, 0f, 1.1f, 0f),
            nudge("↺", 0f, 0f, 1f, -5f), nudge("↻", 0f, 0f, 1f, 5f)
        ))
        addView(row(reset("Pos", 1), reset("Zoom", 2), reset("Rot", 3), reset("Tudo", 0)))

        easingButton.setOnClickListener {
            val next = (NativeEngine.nativeGetCameraEasing() + 1) % easingNames.size
            NativeEngine.nativeSetCameraEasing(next)
            refresh()
        }
        holdCheck.text = "Hold (sem interpolação até a próxima chave)"
        holdCheck.setOnCheckedChangeListener { _, checked ->
            if (!updating) NativeEngine.nativeSetCameraHold(checked)
        }
        pathCheck.text = "Mostrar caminho da câmera"
        pathCheck.isChecked = true
        pathCheck.setOnCheckedChangeListener { _, checked ->
            if (!updating) NativeEngine.nativeSetCameraPathVisible(checked)
        }
        addView(easingButton)
        addView(holdCheck)
        addView(pathCheck)

        viewToggle.setOnClickListener {
            NativeEngine.nativeSetCameraViewMode(!NativeEngine.nativeIsCameraViewMode())
            refresh()
        }
        aspectButton.setOnClickListener {
            val next = (NativeEngine.nativeGetCameraAspectPreset() + 1) % aspectNames.size
            NativeEngine.nativeSetCameraAspectPreset(next)
            refresh()
        }
        keyButton.setOnClickListener {
            if (NativeEngine.nativeHasCameraKey()) NativeEngine.nativeRemoveCameraKey()
            else NativeEngine.nativeSetCameraKey()
            refresh()
        }

        NativeEngine.nativeSetCameraPathVisible(true)
        refresh()
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        handler.post(poller)
    }

    override fun onDetachedFromWindow() {
        handler.removeCallbacks(poller)
        super.onDetachedFromWindow()
    }

    private fun row(vararg views: View): View {
        val row = LinearLayout(context).apply {
            orientation = HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        for (v in views) {
            v.layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
            row.addView(v)
        }
        return row
    }

    private fun nudge(label: String, dx: Float, dy: Float, zoomMul: Float, dRot: Float): Button =
        Button(context).apply {
            text = label
            setOnClickListener {
                NativeEngine.nativeNudgeCamera(dx, dy, zoomMul, dRot)
                refresh()
            }
        }

    private fun reset(label: String, what: Int): Button =
        Button(context).apply {
            text = "Reset $label"
            setOnClickListener {
                NativeEngine.nativeResetCamera(what)
                refresh()
            }
        }

    fun refresh() {
        updating = true
        val cameraView = NativeEngine.nativeIsCameraViewMode()
        viewToggle.text = if (cameraView) "Vista da câmera: ON (desenho pausado)" else "Vista da câmera: OFF"

        val aspect = NativeEngine.nativeGetCameraAspectPreset().coerceIn(0, aspectNames.lastIndex)
        aspectButton.text = "Proporção: ${aspectNames[aspect]}"

        val hasKey = NativeEngine.nativeHasCameraKey()
        keyButton.text = if (hasKey) "◆ Remover chave" else "◇ Criar chave"
        easingButton.isEnabled = hasKey
        holdCheck.isEnabled = hasKey
        val easing = NativeEngine.nativeGetCameraEasing().coerceIn(0, easingNames.lastIndex)
        easingButton.text = "Easing: ${easingNames[easing]}"
        holdCheck.isChecked = hasKey && NativeEngine.nativeGetCameraHold()

        statusLabel.text = "Chaves de câmera: ${NativeEngine.nativeGetCameraKeyCount()} " +
            "(mover/zoom/girar cria chave neste frame)"
        updating = false
    }
}

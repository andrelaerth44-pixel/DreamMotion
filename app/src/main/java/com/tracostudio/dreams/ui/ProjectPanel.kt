package com.tracostudio.dreams.ui

import android.content.Context
import android.graphics.Color
import android.view.Gravity
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import com.tracostudio.dreams.engine.NativeEngine

/**
 * Salvar/carregar o projeto num único slot em armazenamento interno do app
 * (files/current_project.json — caminho decidido pelo MainActivity e passado
 * aqui). Existe também autosave: o MainActivity chama nativeSaveProject no
 * onPause da Activity, então minimizar ou fechar o app não deveria mais
 * perder o trabalho, mesmo sem o usuário tocar em "Salvar".
 *
 * Limitação: um único slot, sem gerenciador de múltiplos projetos/arquivos
 * ainda — é o próximo passo natural se isso virar produto de verdade.
 */
class ProjectPanel(context: Context, private val projectPath: String) : LinearLayout(context) {

    private val statusLabel = TextView(context)

    init {
        orientation = HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        setBackgroundColor(Color.parseColor("#BEBEBE"))
        setPadding(16, 8, 16, 8)

        val saveButton = Button(context).apply {
            text = "\uD83D\uDCBE Salvar"
            setOnClickListener {
                val ok = NativeEngine.nativeSaveProject(projectPath)
                statusLabel.text = if (ok) "Salvo." else "Falha ao salvar."
            }
        }
        val loadButton = Button(context).apply {
            text = "\uD83D\uDCC2 Carregar"
            setOnClickListener {
                val ok = NativeEngine.nativeLoadProject(projectPath)
                statusLabel.text = if (ok) "Carregado." else "Nenhum projeto salvo (ou falha ao ler)."
            }
        }
        addView(saveButton)
        addView(loadButton)
        addView(statusLabel.apply { setPadding(24, 0, 0, 0) })
    }
}

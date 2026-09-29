package com.tracostudio.dreams

import android.app.Activity
import android.os.Bundle
import android.widget.LinearLayout
import android.widget.ScrollView
import com.tracostudio.dreams.canvas.DreamsSurfaceView
import com.tracostudio.dreams.engine.NativeEngine
import com.tracostudio.dreams.ui.BrushPanel
import com.tracostudio.dreams.ui.CameraPanel
import com.tracostudio.dreams.ui.LayersPanel
import com.tracostudio.dreams.ui.ProjectPanel
import java.io.File

class MainActivity : Activity() {

    private lateinit var projectPath: String

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        projectPath = File(filesDir, "current_project.json").absolutePath

        val density = resources.displayMetrics.density
        fun dp(v: Int) = (v * density).toInt()

        val root = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }

        val mainColumn = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.MATCH_PARENT, 1f)
        }

        val projectPanel = ProjectPanel(this, projectPath).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT
            )
        }
        val brushPanel = BrushPanel(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT
            )
        }
        val canvas = DreamsSurfaceView(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f
            )
        }
        val layersPanel = LayersPanel(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT
            )
        }
        mainColumn.addView(projectPanel)
        mainColumn.addView(brushPanel)
        mainColumn.addView(canvas)
        mainColumn.addView(layersPanel)

        val cameraScroll = ScrollView(this).apply {
            layoutParams = LinearLayout.LayoutParams(dp(300), LinearLayout.LayoutParams.MATCH_PARENT)
            addView(CameraPanel(this@MainActivity))
        }

        root.addView(mainColumn)
        root.addView(cameraScroll)
        setContentView(root)
    }

    override fun onPause() {
        super.onPause()
        // Autosave: minimizar ou fechar o app nao deveria perder o trabalho,
        // entao nao dependemos so do botao "Salvar" do ProjectPanel.
        NativeEngine.nativeSaveProject(projectPath)
    }
}

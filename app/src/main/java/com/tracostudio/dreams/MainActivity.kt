package com.tracostudio.dreams

import android.app.Activity
import android.os.Bundle
import android.widget.LinearLayout
import android.widget.ScrollView
import com.tracostudio.dreams.canvas.DreamsSurfaceView
import com.tracostudio.dreams.ui.BrushPanel
import com.tracostudio.dreams.ui.CameraPanel
import com.tracostudio.dreams.ui.LayersPanel

class MainActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val density = resources.displayMetrics.density
        fun dp(v: Int) = (v * density).toInt()

        // Layout em paisagem: coluna principal (pincel / canvas / camadas) + coluna
        // lateral rolável com o painel da câmera.
        val root = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }

        val mainColumn = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.MATCH_PARENT, 1f)
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
}

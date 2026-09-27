package com.tracostudio.dreams

import android.app.Activity
import android.os.Bundle
import android.widget.LinearLayout
import com.tracostudio.dreams.canvas.DreamsSurfaceView
import com.tracostudio.dreams.ui.BrushPanel
import com.tracostudio.dreams.ui.LayersPanel

class MainActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }

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

        root.addView(brushPanel)
        root.addView(canvas)
        root.addView(layersPanel)
        setContentView(root)
    }
}

package com.tracostudio.dreams

import android.app.Activity
import android.os.Bundle
import android.widget.LinearLayout
import com.tracostudio.dreams.canvas.DreamsSurfaceView
import com.tracostudio.dreams.ui.LayersPanel

class MainActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }

        val canvas = DreamsSurfaceView(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f
            )
        }
        val panel = LayersPanel(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT
            )
        }

        root.addView(canvas)
        root.addView(panel)
        setContentView(root)
    }
}

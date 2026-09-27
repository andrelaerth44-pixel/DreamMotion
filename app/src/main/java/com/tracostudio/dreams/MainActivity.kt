package com.tracostudio.dreams

import android.app.Activity
import android.os.Bundle
import com.tracostudio.dreams.canvas.DreamsSurfaceView

class MainActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(DreamsSurfaceView(this))
    }
}

package com.sonicrecomp.app

import android.content.pm.ActivityInfo
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity

/** Editor de controles: arrastra cada control para moverlo y ajusta su tamano. Se edita por separado vertical u horizontal. */
class ControlsEditorActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val land = intent.getBooleanExtra("land", true)
        requestedOrientation = if (land) ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE else ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
        val prefs = Prefs(this)
        val cv = ControlsView(this).apply {
            landscapeMode = land
            editMode = true
            opacity = prefs.controlsOpacity
            loadFrom(prefs)
        }
        val mp = ViewGroup.LayoutParams.MATCH_PARENT
        val wc = ViewGroup.LayoutParams.WRAP_CONTENT
        val info = TextView(this).apply {
            text = if (land) "Modo horizontal: arrastra un control para moverlo; tócalo para ajustar su tamaño."
                   else "Modo vertical: arrastra un control para moverlo; tócalo para ajustar su tamaño."
            setTextColor(Color.WHITE); textSize = 14f; gravity = Gravity.CENTER
        }
        val name = TextView(this).apply { text = "Ningún control seleccionado"; setTextColor(Color.parseColor("#FFD21F")); textSize = 15f; gravity = Gravity.CENTER }
        val seek = SeekBar(this).apply {
            max = 170
            progress = 70
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(s: SeekBar?, p: Int, fromUser: Boolean) { if (fromUser) cv.setSelectedScale(0.3f + p / 100f) }
                override fun onStartTrackingTouch(s: SeekBar?) {}
                override fun onStopTrackingTouch(s: SeekBar?) {}
            })
        }
        cv.onSelect = { g, s ->
            name.text = Ctl.NAMES[g] + "  (tamaño " + (s * 100).toInt() + "%)"
            seek.progress = ((s - 0.3f) * 100).toInt().coerceIn(0, 170)
        }
        val reset = Button(this).apply { text = "Restablecer"; setOnClickListener { cv.resetDefaults() } }
        val cancel = Button(this).apply { text = "Cancelar"; setOnClickListener { finish() } }
        val save = Button(this).apply {
            text = "Guardar"
            setOnClickListener { cv.saveTo(prefs); Toast.makeText(this@ControlsEditorActivity, "Controles guardados", Toast.LENGTH_SHORT).show(); finish() }
        }
        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            addView(reset, LinearLayout.LayoutParams(0, wc, 1f)); addView(cancel, LinearLayout.LayoutParams(0, wc, 1f)); addView(save, LinearLayout.LayoutParams(0, wc, 1f))
        }
        val bar = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.parseColor("#CC000000"))
            setPadding(24, 12, 24, 12)
            addView(info); addView(name); addView(seek); addView(row)
        }
        val root = FrameLayout(this).apply { setBackgroundColor(Color.parseColor("#0A1020")) }
        if (land) {
            root.addView(cv, FrameLayout.LayoutParams(mp, mp))
            root.addView(bar, FrameLayout.LayoutParams(mp, wc, Gravity.TOP))
        } else {
            val col = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
            val top = FrameLayout(this)
            top.addView(bar, FrameLayout.LayoutParams(mp, wc, Gravity.TOP))
            col.addView(top, LinearLayout.LayoutParams(mp, 0, 3f))
            col.addView(cv, LinearLayout.LayoutParams(mp, 0, 2f))
            root.addView(col, FrameLayout.LayoutParams(mp, mp))
        }
        setContentView(root)
        hideSystemBars()
    }
}

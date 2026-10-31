package com.sonicrecomp.app

import android.os.Bundle
import android.view.View
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.SeekBar
import android.widget.Spinner
import android.widget.Switch
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity

/** Ajustes: disposicion de pantallas (tipo DraStic), tamano relativo, botones en pantalla. */
class SettingsActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        title = "Ajustes"
        val prefs = Prefs(this)
        val col = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(48, 48, 48, 48) }
        fun label(t: String) = TextView(this).apply { text = t; textSize = 16f; setPadding(0, 32, 0, 8) }

        col.addView(label("Disposición de pantallas"))
        val spinner = Spinner(this).apply {
            adapter = ArrayAdapter(this@SettingsActivity, android.R.layout.simple_spinner_dropdown_item, Prefs.MODES)
            setSelection(prefs.layoutMode.coerceIn(0, Prefs.MODES.size - 1))
            onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
                override fun onItemSelected(p: AdapterView<*>?, v: View?, pos: Int, id: Long) { prefs.layoutMode = pos }
                override fun onNothingSelected(p: AdapterView<*>?) {}
            }
        }
        col.addView(spinner)

        col.addView(Switch(this).apply {
            text = "Intercambiar pantallas (arriba ↔ abajo)"
            isChecked = prefs.swapped
            setOnCheckedChangeListener { _, on -> prefs.swapped = on }
            setPadding(0, 32, 0, 0)
        })

        val ratioLabel = label("")
        fun ratioText(p: Int) = "Tamaño de la pantalla grande: ${50 + p}% (solo en modos con una pantalla grande)"
        ratioLabel.text = ratioText(((prefs.bigRatio - 0.5f) * 100).toInt().coerceIn(0, 35))
        col.addView(ratioLabel)
        col.addView(SeekBar(this).apply {
            max = 35
            progress = ((prefs.bigRatio - 0.5f) * 100).toInt().coerceIn(0, 35)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(s: SeekBar?, p: Int, fromUser: Boolean) {
                    ratioLabel.text = ratioText(p); prefs.bigRatio = 0.5f + p / 100f
                }
                override fun onStartTrackingTouch(s: SeekBar?) {}
                override fun onStopTrackingTouch(s: SeekBar?) {}
            })
        })

        col.addView(Switch(this).apply {
            text = "Mostrar botones en pantalla"
            isChecked = prefs.showControls
            setOnCheckedChangeListener { _, on -> prefs.showControls = on }
            setPadding(0, 40, 0, 0)
        })

        val opLabel = label("")
        fun opText(p: Int) = "Opacidad de los botones: ${20 + p}%"
        opLabel.text = opText(prefs.controlsOpacity.coerceIn(20, 100) - 20)
        col.addView(opLabel)
        col.addView(SeekBar(this).apply {
            max = 80
            progress = prefs.controlsOpacity.coerceIn(20, 100) - 20
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(s: SeekBar?, p: Int, fromUser: Boolean) {
                    opLabel.text = opText(p); prefs.controlsOpacity = 20 + p
                }
                override fun onStartTrackingTouch(s: SeekBar?) {}
                override fun onStopTrackingTouch(s: SeekBar?) {}
            })
        })

        col.addView(TextView(this).apply {
            text = "Dentro del juego, el botón ⇄ de los controles intercambia las pantallas al instante."
            setPadding(0, 48, 0, 0)
        })
        setContentView(ScrollView(this).apply { addView(col) })
    }
}

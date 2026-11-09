package com.sonicrecomp.app

import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.SeekBar
import android.widget.Spinner
import android.widget.Switch
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity

/** Ajustes: pantallas, calidad, orientacion, rendimiento y controles en pantalla (con editor de posicion y tamano). */
class SettingsActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        title = "Ajustes"
        val prefs = Prefs(this)
        val col = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(48, 48, 48, 48) }
        fun label(t: String) = TextView(this).apply { text = t; textSize = 16f; setPadding(0, 32, 0, 8) }
        fun spinner(items: Array<String>, selected: Int, onPick: (Int) -> Unit) = Spinner(this).apply {
            adapter = ArrayAdapter(this@SettingsActivity, android.R.layout.simple_spinner_dropdown_item, items)
            setSelection(selected.coerceIn(0, items.size - 1))
            onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
                override fun onItemSelected(p: AdapterView<*>?, v: View?, pos: Int, id: Long) { onPick(pos) }
                override fun onNothingSelected(p: AdapterView<*>?) {}
            }
        }

        col.addView(label("Orientación del juego"))
        col.addView(spinner(Prefs.ORIENTATIONS, prefs.orientation) { prefs.orientation = it })

        col.addView(label("Disposición de pantallas"))
        col.addView(spinner(Prefs.MODES, prefs.layoutMode) { prefs.layoutMode = it })

        col.addView(Switch(this).apply {
            text = "Intercambiar pantallas (arriba ↔ abajo / izquierda ↔ derecha)"
            isChecked = prefs.swapped
            setOnCheckedChangeListener { _, on -> prefs.swapped = on }
            setPadding(0, 32, 0, 0)
        })

        val ratioLabel = label("")
        fun ratioText(p: Int) = "Tamaño de la pantalla grande: ${50 + p}% (solo en modos con una pantalla grande)"
        val r0 = ((prefs.bigRatio - 0.5f) * 100).toInt().coerceIn(0, 35)
        ratioLabel.text = ratioText(r0)
        col.addView(ratioLabel)
        col.addView(SeekBar(this).apply {
            max = 35
            progress = r0
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(s: SeekBar?, p: Int, fromUser: Boolean) { ratioLabel.text = ratioText(p); prefs.bigRatio = 0.5f + p / 100f }
                override fun onStartTrackingTouch(s: SeekBar?) {}
                override fun onStopTrackingTouch(s: SeekBar?) {}
            })
        })

        col.addView(label("Resolución interna del 3D"))
        col.addView(spinner(Prefs.SCALES, prefs.renderScale - 1) { prefs.renderScale = it + 1 })
        col.addView(label("Filtro de imagen"))
        col.addView(spinner(Prefs.FILTERS, prefs.filter) { prefs.filter = it })

        col.addView(Switch(this).apply {
            text = "Salto de fotogramas automático (más fluido en móviles lentos)"
            isChecked = prefs.frameskip
            setOnCheckedChangeListener { _, on -> prefs.frameskip = on }
            setPadding(0, 32, 0, 0)
        })

        col.addView(label("Controles en pantalla"))
        col.addView(Switch(this).apply {
            text = "Mostrar botones en pantalla"
            isChecked = prefs.showControls
            setOnCheckedChangeListener { _, on -> prefs.showControls = on }
        })
        val opLabel = label("")
        fun opText(p: Int) = "Opacidad de los botones: ${20 + p}%"
        opLabel.text = opText(prefs.controlsOpacity.coerceIn(20, 100) - 20)
        col.addView(opLabel)
        col.addView(SeekBar(this).apply {
            max = 80
            progress = prefs.controlsOpacity.coerceIn(20, 100) - 20
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(s: SeekBar?, p: Int, fromUser: Boolean) { opLabel.text = opText(p); prefs.controlsOpacity = 20 + p }
                override fun onStartTrackingTouch(s: SeekBar?) {}
                override fun onStopTrackingTouch(s: SeekBar?) {}
            })
        })
        col.addView(Button(this).apply {
            text = "Editar posición y tamaño — modo horizontal"
            setOnClickListener { startActivity(Intent(this@SettingsActivity, ControlsEditorActivity::class.java).putExtra("land", true)) }
        })
        col.addView(Button(this).apply {
            text = "Editar posición y tamaño — modo vertical"
            setOnClickListener { startActivity(Intent(this@SettingsActivity, ControlsEditorActivity::class.java).putExtra("land", false)) }
        })
        col.addView(Button(this).apply {
            text = "Restablecer todos los controles"
            setOnClickListener { prefs.resetCtl(true); prefs.resetCtl(false) }
        })
        col.addView(TextView(this).apply {
            text = "Dentro del juego, el botón ⇄ intercambia las pantallas al instante."
            setPadding(0, 48, 0, 0)
        })
        setContentView(ScrollView(this).apply { addView(col) })
    }
}

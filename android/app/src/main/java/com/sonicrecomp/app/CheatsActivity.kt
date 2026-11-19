package com.sonicrecomp.app

import android.net.Uri
import android.os.Bundle
import android.text.InputType
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity

/** Menu de trucos Action Replay: importar archivo, anadir a mano, activar/desactivar, borrar. */
class CheatsActivity : AppCompatActivity() {
    private lateinit var store: CheatStore
    private lateinit var list: LinearLayout

    private val picker = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) importFile(uri)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        title = "Trucos"
        store = CheatStore(this)
        val col = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(48, 48, 48, 48) }
        col.addView(Button(this).apply { text = "Importar archivo de trucos"; setOnClickListener { picker.launch(arrayOf("*/*")) } })
        col.addView(Button(this).apply { text = "Añadir truco manualmente"; setOnClickListener { addDialog() } })
        col.addView(TextView(this).apply {
            text = "Usa los mismos códigos Action Replay que en DraStic. Funcionan los que cambian datos " +
                "(vidas, anillos, vida…). Los que parchean el código del juego no pueden funcionar en un recomp. " +
                "Los cambios se aplican al iniciar el juego."
            setPadding(0, 24, 0, 24)
        })
        list = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        col.addView(list)
        setContentView(ScrollView(this).apply { addView(col) })
        refresh()
    }

    private fun refresh() {
        list.removeAllViews()
        if (store.items.isEmpty()) {
            list.addView(TextView(this).apply { text = "Aún no hay trucos." })
            return
        }
        store.items.forEach { item ->
            list.addView(CheckBox(this).apply {
                text = item.name
                isChecked = item.enabled
                setPadding(0, 16, 0, 16)
                setOnCheckedChangeListener { _, on -> item.enabled = on; store.save() }
                setOnLongClickListener {
                    AlertDialog.Builder(this@CheatsActivity)
                        .setTitle("¿Borrar \"${item.name}\"?")
                        .setPositiveButton("Borrar") { _, _ -> store.items.remove(item); store.save(); refresh() }
                        .setNegativeButton("Cancelar", null).show()
                    true
                }
            })
        }
        list.addView(TextView(this).apply { text = "Mantén pulsado un truco para borrarlo."; setPadding(0, 24, 0, 0) })
    }

    private fun addDialog() {
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(48, 24, 48, 0) }
        val name = EditText(this).apply { hint = "Nombre del truco" }
        val code = EditText(this).apply {
            hint = "94000130 FFFB0000\n02000000 00000063"
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_MULTI_LINE or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
            minLines = 4
        }
        box.addView(name); box.addView(code)
        AlertDialog.Builder(this).setTitle("Nuevo truco").setView(box)
            .setPositiveButton("Añadir") { _, _ ->
                val c = code.text.toString()
                if (!NativeBridge.cheatsValidate(c)) {
                    Toast.makeText(this, "Código no válido: cada línea son 2 bloques de 8 dígitos hexadecimales", Toast.LENGTH_LONG).show()
                } else {
                    val n = name.text.toString().ifBlank { "Truco ${store.items.size + 1}" }
                    store.add(CheatItem(n, c.trim(), false)); refresh()
                }
            }.setNegativeButton("Cancelar", null).show()
    }

    private fun importFile(uri: Uri) {
        try {
            val limit = 2 * 1024 * 1024   // los archivos de trucos de texto son pequenos
            val buf = java.io.ByteArrayOutputStream()
            contentResolver.openInputStream(uri)!!.use { ins ->
                val tmp = ByteArray(8192)
                while (buf.size() < limit) {
                    val n = ins.read(tmp, 0, minOf(tmp.size, limit - buf.size()))
                    if (n < 0) break
                    buf.write(tmp, 0, n)
                }
            }
            val bytes = buf.toByteArray()
            val arr = NativeBridge.cheatsParseFile(String(bytes, Charsets.UTF_8))
            var added = 0
            for (i in 0 until arr.size step 3) if (store.add(CheatItem(arr[i], arr[i + 1], arr[i + 2] == "1"))) added++
            Toast.makeText(this,
                if (arr.isEmpty()) "No se encontraron trucos en ese archivo (¿es de texto?)" else "Importados $added trucos",
                Toast.LENGTH_LONG).show()
            refresh()
        } catch (e: Exception) {
            Toast.makeText(this, "No se pudo leer el archivo", Toast.LENGTH_LONG).show()
        }
    }
}

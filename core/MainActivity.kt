package com.sonicrecomp.app

import android.content.Intent
import android.graphics.Color
import android.net.Uri
import android.os.Bundle
import android.view.Gravity
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import java.io.File
import kotlin.concurrent.thread

/** Menu principal: eliges tu ROM, la app la verifica y extrae, y le das a Jugar. */
class MainActivity : AppCompatActivity() {
    private lateinit var status: TextView
    private lateinit var bar: ProgressBar
    private lateinit var pick: Button
    private lateinit var play: Button
    private val gameDir by lazy { File(filesDir, "game") }
    private val okMarker by lazy { File(gameDir, ".extracted_ok") }

    private val picker = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) importRom(uri)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(64, 64, 64, 64)
            setBackgroundColor(Color.parseColor("#0A1F44"))
        }
        val title = TextView(this).apply {
            text = "SONIC COLORS"; textSize = 30f; setTextColor(Color.WHITE); gravity = Gravity.CENTER
        }
        val sub = TextView(this).apply {
            text = "Recomp para Android\nNecesitas tu propia ROM (USA)"
            setTextColor(Color.LTGRAY); gravity = Gravity.CENTER; setPadding(0, 16, 0, 48)
        }
        pick = Button(this).apply { text = "Elegir ROM (.nds)"; setOnClickListener { picker.launch(arrayOf("*/*")) } }
        play = Button(this).apply {
            text = "JUGAR"; setOnClickListener { startActivity(Intent(this@MainActivity, GameActivity::class.java)) }
        }
        bar = ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal).apply { max = 100; visibility = ProgressBar.GONE }
        status = TextView(this).apply { setTextColor(Color.WHITE); gravity = Gravity.CENTER; setPadding(0, 32, 0, 0) }
        val settings = Button(this).apply {
            text = "Ajustes (pantallas y botones)"
            setOnClickListener { startActivity(Intent(this@MainActivity, SettingsActivity::class.java)) }
        }
        val cheats = Button(this).apply {
            text = "Trucos"
            setOnClickListener { startActivity(Intent(this@MainActivity, CheatsActivity::class.java)) }
        }
        listOf(title, sub, pick, play, settings, cheats, bar, status).forEach { root.addView(it) }
        setContentView(root)
        refresh()
    }

    private fun refresh() {
        val ready = okMarker.exists()
        play.isEnabled = ready
        status.text = if (ready) "Juego listo" else "Elige tu ROM para empezar"
    }

    private fun importRom(uri: Uri) {
        pick.isEnabled = false; play.isEnabled = false
        bar.visibility = ProgressBar.VISIBLE; bar.progress = 0
        thread {
            val tmp = File(cacheDir, "rom.nds")
            var err = ""
            try {
                runOnUiThread { status.text = "Copiando ROM..." }
                contentResolver.openInputStream(uri)!!.use { i -> tmp.outputStream().use { o -> i.copyTo(o) } }
                gameDir.deleteRecursively(); okMarker.delete()
                err = NativeBridge.extractRom(tmp.absolutePath, gameDir.absolutePath, object : ProgressCallback {
                    override fun onProgress(percent: Int, msg: String) = runOnUiThread {
                        bar.progress = percent; status.text = msg
                    }
                })
                if (err.isEmpty()) okMarker.writeText("ok")
            } catch (e: Exception) {
                err = e.message ?: "Error leyendo la ROM"
            } finally {
                tmp.delete()   // no guardamos copia de la ROM
            }
            runOnUiThread {
                bar.visibility = ProgressBar.GONE
                pick.isEnabled = true
                if (err.isNotEmpty()) status.text = "Error: $err"
                refresh()
            }
        }
    }
}

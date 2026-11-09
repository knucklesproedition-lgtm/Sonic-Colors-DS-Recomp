package com.sonicrecomp.app

import android.content.Intent
import android.content.res.ColorStateList
import android.content.res.Configuration
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.graphics.drawable.RippleDrawable
import android.net.Uri
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.ScrollView
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import java.io.File
import kotlin.concurrent.thread

/** Menu principal: panel de titulo a la izquierda y botones en pastilla a la derecha (en vertical, apilados). */
class MainActivity : AppCompatActivity() {
    private var status: TextView? = null
    private var bar: ProgressBar? = null
    private var busy = false
    private var lastMsg = ""
    private val gameDir by lazy { File(filesDir, "game") }
    private val okMarker by lazy { File(gameDir, ".extracted_ok") }

    private val picker = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) importRom(uri)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        buildUi()
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        buildUi()
    }

    override fun onResume() {
        super.onResume()
        if (!busy) buildUi()                                   // por si cambio el estado de la ROM
    }

    private fun dp(v: Int): Int = (v * resources.displayMetrics.density).toInt()

    private fun pill(label: String, primary: Boolean, enabled: Boolean = true, onClick: () -> Unit): TextView {
        val bg = GradientDrawable()
        bg.cornerRadius = dp(30).toFloat()
        if (primary) {
            bg.orientation = GradientDrawable.Orientation.TOP_BOTTOM
            bg.colors = intArrayOf(Color.parseColor("#FFC93A"), Color.parseColor("#FF9D00"))
            bg.setStroke(dp(2), Color.parseColor("#FFE9A6"))
        } else {
            bg.setColor(Color.parseColor("#0D1A3A"))
            bg.setStroke(dp(2), Color.parseColor("#2F6BFF"))
        }
        val ripple = RippleDrawable(ColorStateList.valueOf(Color.parseColor("#44FFFFFF")), bg, null)
        return TextView(this).apply {
            text = label
            gravity = Gravity.CENTER
            textSize = 16f
            isAllCaps = true
            letterSpacing = 0.08f
            typeface = Typeface.create("sans-serif-medium", Typeface.NORMAL)
            setTextColor(if (primary) Color.parseColor("#1B2440") else Color.WHITE)
            background = ripple
            alpha = if (enabled) 1f else 0.4f
            isClickable = enabled
            if (enabled) setOnClickListener { onClick() }
        }
    }

    private fun buildUi() {
        val land = resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE
        val ready = okMarker.exists()
        window.statusBarColor = Color.parseColor("#050A18")
        window.navigationBarColor = Color.parseColor("#050A18")

        val root = FrameLayout(this)
        root.background = GradientDrawable(GradientDrawable.Orientation.TL_BR,
            intArrayOf(Color.parseColor("#0C1C42"), Color.parseColor("#08122B"), Color.parseColor("#040813")))

        // ---- panel izquierdo: titulo ----
        val title = TextView(this).apply {
            text = "Sonic Colors"
            textSize = if (land) 44f else 38f
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            setTextColor(Color.parseColor("#FFD21F"))
            setShadowLayer(10f, 0f, 4f, Color.parseColor("#66000000"))
        }
        val subtitle = TextView(this).apply {
            text = "DS RECOMP · ANDROID EDITION"
            textSize = 13f
            letterSpacing = 0.22f
            typeface = Typeface.create("sans-serif-medium", Typeface.NORMAL)
            setTextColor(Color.parseColor("#7FB6FF"))
            setPadding(0, dp(6), 0, dp(14))
        }
        val divider = View(this).apply { setBackgroundColor(Color.parseColor("#2F6BFF")) }
        val st = TextView(this).apply {
            textSize = 14f
            setPadding(0, dp(16), 0, 0)
            if (busy) { text = lastMsg; setTextColor(Color.parseColor("#FFC93A")) }
            else if (ready) { text = "✓ Juego listo — ¡a jugar!"; setTextColor(Color.parseColor("#7CFFB2")) }
            else { text = "▲ Falta la ROM — toca ELEGIR ROM para empezar"; setTextColor(Color.parseColor("#FFC93A")) }
        }
        status = st
        val pb = ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal).apply {
            max = 100
            visibility = if (busy) View.VISIBLE else View.GONE
        }
        bar = pb
        val left = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = if (land) (Gravity.CENTER_VERTICAL or Gravity.START) else Gravity.CENTER_HORIZONTAL
            addView(title)
            addView(subtitle)
            addView(divider, LinearLayout.LayoutParams(dp(120), dp(3)))
            addView(st)
            addView(pb, LinearLayout.LayoutParams(dp(220), ViewGroup.LayoutParams.WRAP_CONTENT))
        }

        // ---- panel derecho: botones ----
        val right = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; gravity = Gravity.CENTER }
        fun addBtn(v: View) {
            val lp = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(56))
            lp.bottomMargin = dp(14)
            right.addView(v, lp)
        }
        if (ready) {
            addBtn(pill("Jugar", true, !busy) { startActivity(Intent(this, GameActivity::class.java)) })
        } else {
            addBtn(pill("Elegir ROM (.nds)", true, !busy) { picker.launch(arrayOf("*/*")) })
        }
        addBtn(pill("Ajustes", false, !busy) { startActivity(Intent(this, SettingsActivity::class.java)) })
        addBtn(pill("Trucos", false, !busy) { startActivity(Intent(this, CheatsActivity::class.java)) })
        if (ready) addBtn(pill("Elegir otra ROM", false, !busy) { picker.launch(arrayOf("*/*")) })

        val panel = LinearLayout(this).apply {
            orientation = if (land) LinearLayout.HORIZONTAL else LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(dp(36), dp(24), dp(36), dp(56))
        }
        if (land) {
            panel.addView(left, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
            val lp = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
            lp.leftMargin = dp(48)
            panel.addView(right, lp)
        } else {
            val lpL = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            lpL.bottomMargin = dp(36)
            panel.addView(left, lpL)
            panel.addView(right, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
        }
        val scroll = ScrollView(this).apply { isFillViewport = true; addView(panel, ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)) }
        root.addView(scroll, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        val footer = TextView(this).apply {
            text = "v1.2 · Sonic Colors DS Recomp — necesitas tu propia ROM"
            textSize = 11f
            setTextColor(Color.parseColor("#6B7A99"))
        }
        root.addView(footer, FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL).apply { bottomMargin = dp(16) })
        setContentView(root)
    }

    private fun importRom(uri: Uri) {
        busy = true; lastMsg = "Copiando ROM…"
        buildUi()
        thread {
            val tmp = File(cacheDir, "rom.nds")
            var err = ""
            try {
                contentResolver.openInputStream(uri)!!.use { i -> tmp.outputStream().use { o -> i.copyTo(o) } }
                val sav = File(gameDir, "save.sav"); val savBytes = if (sav.exists()) sav.readBytes() else null   // no perder la partida
                gameDir.deleteRecursively(); okMarker.delete()
                err = NativeBridge.extractRom(tmp.absolutePath, gameDir.absolutePath, object : ProgressCallback {
                    override fun onProgress(percent: Int, msg: String) = runOnUiThread {
                        lastMsg = msg
                        bar?.progress = percent; status?.text = msg
                    }
                })
                if (err.isEmpty()) {
                    // El juego lee overlays del "cartucho" mientras corre: se guarda una copia PRIVADA de tu ROM
                    // en el almacenamiento interno de la app (nadie mas puede leerla; se borra al desinstalar).
                    tmp.copyTo(File(gameDir, "rom.nds"), overwrite = true)
                    savBytes?.let { File(gameDir, "save.sav").writeBytes(it) }
                    okMarker.writeText("ok")
                }
            } catch (e: Exception) {
                err = e.message ?: "Error leyendo la ROM"
            } finally {
                tmp.delete()   // no dejamos copias sueltas en la cache
            }
            runOnUiThread {
                busy = false
                buildUi()
                if (err.isNotEmpty()) { status?.text = "Error: $err"; status?.setTextColor(Color.parseColor("#FF6B6B")) }
            }
        }
    }
}

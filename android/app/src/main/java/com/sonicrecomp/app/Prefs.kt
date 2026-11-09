package com.sonicrecomp.app

import android.content.Context

/** Ajustes guardados. Los modos de pantalla coinciden con nds::LayoutMode de C++. */
class Prefs(ctx: Context) {
    private val sp = ctx.getSharedPreferences("settings", Context.MODE_PRIVATE)

    var layoutMode: Int
        get() = sp.getInt("layoutMode", 0)
        set(v) = sp.edit().putInt("layoutMode", v).apply()
    var swapped: Boolean
        get() = sp.getBoolean("swapped", false)
        set(v) = sp.edit().putBoolean("swapped", v).apply()
    /** Fraccion de la pantalla para la pantalla grande (0.50 a 0.85) */
    var bigRatio: Float
        get() = sp.getFloat("bigRatio", 0.65f)
        set(v) = sp.edit().putFloat("bigRatio", v).apply()
    var showControls: Boolean
        get() = sp.getBoolean("showControls", true)
        set(v) = sp.edit().putBoolean("showControls", v).apply()
    var controlsOpacity: Int
        get() = sp.getInt("controlsOpacity", 70)
        set(v) = sp.edit().putInt("controlsOpacity", v).apply()
    /** Resolucion interna del 3D: 1 a 4 veces. 1x por defecto: es la mas fluida en moviles. */
    var renderScale: Int
        get() = sp.getInt("renderScale", 1)
        set(v) = sp.edit().putInt("renderScale", v.coerceIn(1, 4)).apply()
    /** 0 = pixeles, 1 = suave (bilinear), 2 = nitido (bilinear nitido) */
    var filter: Int
        get() = sp.getInt("filter", 2)
        set(v) = sp.edit().putInt("filter", v.coerceIn(0, 2)).apply()
    /** 0 = automatica, 1 = vertical, 2 = horizontal */
    var orientation: Int
        get() = sp.getInt("orientation", 0)
        set(v) = sp.edit().putInt("orientation", v).apply()
    /** Salto de fotogramas automatico (video/3D): mas fluidez en moviles lentos. */
    var frameskip: Boolean
        get() = sp.getBoolean("frameskip", true)
        set(v) = sp.edit().putBoolean("frameskip", v).apply()

    // Posicion (fraccion de la vista) y tamano de cada control, por separado en vertical y horizontal
    private fun ck(land: Boolean, g: Int, f: String) = "c" + (if (land) "l" else "p") + g + f
    fun ctlX(land: Boolean, g: Int): Float = sp.getFloat(ck(land, g, "x"), Ctl.defX(land, g))
    fun ctlY(land: Boolean, g: Int): Float = sp.getFloat(ck(land, g, "y"), Ctl.defY(land, g))
    fun ctlS(land: Boolean, g: Int): Float = sp.getFloat(ck(land, g, "s"), Ctl.defS(land, g))
    fun setCtl(land: Boolean, g: Int, x: Float, y: Float, s: Float) {
        sp.edit().putFloat(ck(land, g, "x"), x).putFloat(ck(land, g, "y"), y).putFloat(ck(land, g, "s"), s).apply()
    }
    fun resetCtl(land: Boolean) {
        val e = sp.edit()
        for (g in 0 until Ctl.COUNT) { e.remove(ck(land, g, "x")); e.remove(ck(land, g, "y")); e.remove(ck(land, g, "s")) }
        e.apply()
    }

    fun applyToEngine() {
        NativeBridge.engineSetLayout(layoutMode, swapped, bigRatio)
        NativeBridge.engineSetGraphics(renderScale, filter)
        NativeBridge.engineSetFrameskip(frameskip)
    }

    companion object {
        val MODES = arrayOf(
            "Dos pantallas iguales",
            "Superior grande, inferior pequeña",
            "Inferior (táctil) grande, superior pequeña",
            "Solo pantalla superior",
            "Solo pantalla inferior"
        )
        val SCALES = arrayOf("1x (nativa, la más fluida)", "2x (512×384)", "3x (768×576)", "4x (1024×768, lento)")
        val FILTERS = arrayOf("Píxeles (sin suavizar)", "Suave (bilineal)", "Nítido (bilineal nítido)")
        val ORIENTATIONS = arrayOf("Automática (según el sensor)", "Vertical", "Horizontal")
    }
}

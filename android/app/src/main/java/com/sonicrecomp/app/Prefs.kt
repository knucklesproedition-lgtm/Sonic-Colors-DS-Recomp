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
    /** Fraccion de altura para la pantalla grande (0.50 a 0.85) */
    var bigRatio: Float
        get() = sp.getFloat("bigRatio", 0.65f)
        set(v) = sp.edit().putFloat("bigRatio", v).apply()
    var showControls: Boolean
        get() = sp.getBoolean("showControls", true)
        set(v) = sp.edit().putBoolean("showControls", v).apply()
    /** Opacidad de los botones, 20..100 */
    var controlsOpacity: Int
        get() = sp.getInt("controlsOpacity", 70)
        set(v) = sp.edit().putInt("controlsOpacity", v).apply()

    /** Resolucion interna del 3D: 1 a 4 veces (256x192 x N). Mas alto = mas nitido y mas lento. */
    var renderScale: Int
        get() = sp.getInt("renderScale", 2)
        set(v) = sp.edit().putInt("renderScale", v.coerceIn(1, 4)).apply()
    /** 0 = pixeles, 1 = suave (bilinear), 2 = nitido (bilinear nitido) */
    var filter: Int
        get() = sp.getInt("filter", 2)
        set(v) = sp.edit().putInt("filter", v.coerceIn(0, 2)).apply()

    fun applyToEngine() {
        NativeBridge.engineSetLayout(layoutMode, swapped, bigRatio)
        NativeBridge.engineSetGraphics(renderScale, filter)
    }

    companion object {
        val SCALES = arrayOf("1x (nativa, 256×192)", "2x (512×384)", "3x (768×576)", "4x (1024×768, lento)")
        val FILTERS = arrayOf("Píxeles (sin suavizar)", "Suave (bilineal)", "Nítido (bilineal nítido)")
        val MODES = arrayOf(
            "Dos pantallas iguales",
            "Superior grande, inferior pequeña",
            "Inferior (táctil) grande, superior pequeña",
            "Solo pantalla superior",
            "Solo pantalla inferior"
        )
    }
}

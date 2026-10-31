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

    fun applyToEngine() = NativeBridge.engineSetLayout(layoutMode, swapped, bigRatio)

    companion object {
        val MODES = arrayOf(
            "Dos pantallas iguales",
            "Superior grande, inferior pequeña",
            "Inferior (táctil) grande, superior pequeña",
            "Solo pantalla superior",
            "Solo pantalla inferior"
        )
    }
}

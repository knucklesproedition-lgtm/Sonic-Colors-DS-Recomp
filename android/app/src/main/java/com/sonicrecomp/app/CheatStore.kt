package com.sonicrecomp.app

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

data class CheatItem(var name: String, var code: String, var enabled: Boolean)

/** Lista de trucos guardada en un JSON interno de la app. */
class CheatStore(ctx: Context) {
    companion object {
        const val DEFAULT_SINGLE_SCREEN_NAME = "Pantalla única (Fix main 2D screen) - Sonic Colors DS NTSC"
        // Solo parchea si la memoria tiene la instruccion original (ARM9 ya descomprimido); el motor ademas espera al fin del arranque.
        const val DEFAULT_SINGLE_SCREEN_CODE = "5202B738 E5923028\n0202A558 03A0047F\n0202A574 03A0047F\n020144B8 E1A00000\n0202B634 E1A01003\nD2000000 00000000"
    }
    private val file = File(ctx.filesDir, "cheats.json")
    val items = mutableListOf<CheatItem>()

    init {
        load()
        if (!file.exists()) {                                  // primera vez: trucos de serie (desactivados)
            items += CheatItem(DEFAULT_SINGLE_SCREEN_NAME, DEFAULT_SINGLE_SCREEN_CODE, false)
            save()
        }
    }

    private fun load() {
        items.clear()
        if (!file.exists()) return
        try {
            val arr = JSONArray(file.readText())
            for (i in 0 until arr.length()) {
                val o = arr.getJSONObject(i)
                items += CheatItem(o.getString("name"), o.getString("code"), o.optBoolean("enabled", false))
            }
        } catch (_: Exception) { items.clear() }
    }

    fun save() {
        val arr = JSONArray()
        items.forEach { arr.put(JSONObject().put("name", it.name).put("code", it.code).put("enabled", it.enabled)) }
        file.writeText(arr.toString())
    }

    /** Anade evitando duplicados exactos (mismo nombre y codigo). */
    fun add(item: CheatItem): Boolean {
        if (items.any { it.name == item.name && it.code == item.code }) return false
        items += item; save(); return true
    }

    /** Envia los trucos activados al motor nativo. */
    fun pushToEngine() {
        NativeBridge.cheatsClear()
        items.filter { it.enabled }.forEach { NativeBridge.cheatsAdd(it.name, it.code, true) }
    }
}

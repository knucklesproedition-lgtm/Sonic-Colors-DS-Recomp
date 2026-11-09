package com.sonicrecomp.app

interface ProgressCallback { fun onProgress(percent: Int, msg: String) }

object NativeBridge {
    init { System.loadLibrary("sonicrecomp") }

    // ROM
    @JvmStatic external fun extractRom(romPath: String, outDir: String, cb: ProgressCallback): String

    // Motor
    @JvmStatic external fun engineInit(dataDir: String)
    @JvmStatic external fun engineFrame()
    @JvmStatic external fun engineFlush()
    @JvmStatic external fun engineGlInit()
    @JvmStatic external fun engineSetViewport(w: Int, h: Int)
    @JvmStatic external fun engineSetGraphics(scale: Int, filter: Int)
    @JvmStatic external fun engineSetFrameskip(on: Boolean)
    @JvmStatic external fun engineIsRunning(): Boolean
    @JvmStatic external fun engineSetKeys(mask: Int)
    @JvmStatic external fun engineTouchView(viewW: Float, viewH: Float, x: Float, y: Float, down: Boolean)

    // Audio: estereo de 16 bits a 32728 Hz; devuelve cuantos fotogramas de audio copio
    @JvmStatic external fun engineReadAudio(buf: ShortArray, frames: Int): Int

    // Pantallas: [top x,y,w,h,visible, bottom x,y,w,h,visible]
    @JvmStatic external fun engineSetLayout(mode: Int, swapped: Boolean, ratio: Float)
    @JvmStatic external fun engineGetLayout(viewW: Float, viewH: Float): FloatArray

    // Mods
    @JvmStatic external fun modsSetSuperSonic(on: Boolean)

    // Trucos
    @JvmStatic external fun cheatsClear()
    @JvmStatic external fun cheatsAdd(name: String, code: String, enabled: Boolean): Boolean
    @JvmStatic external fun cheatsValidate(code: String): Boolean
    @JvmStatic external fun cheatsParseFile(text: String): Array<String>
}

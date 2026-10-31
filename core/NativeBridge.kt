package com.sonicrecomp.app

interface ProgressCallback { fun onProgress(percent: Int, msg: String) }

object NativeBridge {
    init { System.loadLibrary("sonicrecomp") }

    // ROM
    @JvmStatic external fun extractRom(romPath: String, outDir: String, cb: ProgressCallback): String

    // Motor
    @JvmStatic external fun engineInit(dataDir: String)
    @JvmStatic external fun engineFrame()
    @JvmStatic external fun engineSetKeys(mask: Int)
    @JvmStatic external fun engineTouchView(viewW: Float, viewH: Float, x: Float, y: Float, down: Boolean)

    // Pantallas: [top x,y,w,h,visible, bottom x,y,w,h,visible]
    @JvmStatic external fun engineSetLayout(mode: Int, swapped: Boolean, ratio: Float)
    @JvmStatic external fun engineGetLayout(viewW: Float, viewH: Float): FloatArray

    // Trucos
    @JvmStatic external fun cheatsClear()
    @JvmStatic external fun cheatsAdd(name: String, code: String, enabled: Boolean): Boolean
    @JvmStatic external fun cheatsValidate(code: String): Boolean
    @JvmStatic external fun cheatsParseFile(text: String): Array<String>
}

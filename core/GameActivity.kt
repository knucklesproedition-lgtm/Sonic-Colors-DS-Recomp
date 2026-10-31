package com.sonicrecomp.app

import android.opengl.GLES30
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.view.MotionEvent
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.LinearLayout
import androidx.appcompat.app.AppCompatActivity
import java.io.File
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

/** Pantalla de juego: pantallas de DS (segun ajustes) arriba y botones abajo. */
class GameActivity : AppCompatActivity() {
    private lateinit var prefs: Prefs
    private lateinit var gl: GLSurfaceView
    private var controls: ControlsView? = null

    @Volatile private var viewW = 1
    @Volatile private var viewH = 1
    @Volatile private var layoutDirty = true
    private var layoutArr = FloatArray(10)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        prefs = Prefs(this)
        prefs.applyToEngine()
        CheatStore(this).pushToEngine()                 // trucos activados en el menu
        NativeBridge.engineInit(File(filesDir, "game").absolutePath)

        gl = object : GLSurfaceView(this) {
            override fun onTouchEvent(e: MotionEvent): Boolean {
                val down = e.actionMasked != MotionEvent.ACTION_UP && e.actionMasked != MotionEvent.ACTION_CANCEL
                NativeBridge.engineTouchView(viewW.toFloat(), viewH.toFloat(), e.x, e.y, down)
                return true
            }
        }.apply {
            setEGLContextClientVersion(3)
            setRenderer(object : GLSurfaceView.Renderer {
                override fun onSurfaceCreated(gl: GL10?, c: EGLConfig?) {}
                override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
                    viewW = width; viewH = height; layoutDirty = true
                    GLES30.glViewport(0, 0, width, height)
                }
                override fun onDrawFrame(gl: GL10?) {
                    if (layoutDirty) {
                        layoutArr = NativeBridge.engineGetLayout(viewW.toFloat(), viewH.toFloat())
                        layoutDirty = false
                    }
                    GLES30.glDisable(GLES30.GL_SCISSOR_TEST)
                    GLES30.glClearColor(0.02f, 0.03f, 0.08f, 1f)
                    GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT)
                    // Marcadores de posicion hasta que el motor dibuje las pantallas reales
                    fill(0, 0.10f, 0.25f, 0.55f)    // superior de DS
                    fill(5, 0.10f, 0.45f, 0.30f)    // inferior (tactil)
                    NativeBridge.engineFrame()
                }
            })
        }

        val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        root.addView(gl, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 3f))
        if (prefs.showControls) {
            controls = ControlsView(this).apply {
                opacity = prefs.controlsOpacity
                onKeys = { mask -> NativeBridge.engineSetKeys(mask) }
                onSwap = {
                    prefs.swapped = !prefs.swapped
                    prefs.applyToEngine()
                    layoutDirty = true
                }
            }
            root.addView(controls, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 2f))
        }
        setContentView(root)
    }

    /** Rellena el rectangulo del arreglo de layout en la posicion [off] (0 = superior, 5 = inferior). */
    private fun fill(off: Int, r: Float, g: Float, b: Float) {
        if (layoutArr[off + 4] < 0.5f) return
        val x = layoutArr[off].toInt(); val w = layoutArr[off + 2].toInt(); val h = layoutArr[off + 3].toInt()
        val glY = (viewH - (layoutArr[off + 1] + layoutArr[off + 3])).toInt()   // GL cuenta desde abajo
        GLES30.glEnable(GLES30.GL_SCISSOR_TEST)
        GLES30.glScissor(x, glY, w, h)
        GLES30.glClearColor(r, g, b, 1f)
        GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT)
        GLES30.glDisable(GLES30.GL_SCISSOR_TEST)
    }

    override fun onPause() { super.onPause(); gl.onPause(); NativeBridge.engineSetKeys(0) }
    override fun onResume() { super.onResume(); gl.onResume() }
}

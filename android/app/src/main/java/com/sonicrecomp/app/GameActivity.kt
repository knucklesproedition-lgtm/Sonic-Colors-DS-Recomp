package com.sonicrecomp.app

import android.content.pm.ActivityInfo
import android.content.res.Configuration
import android.opengl.GLES30
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.view.MotionEvent
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.LinearLayout
import androidx.appcompat.app.AppCompatActivity
import java.io.File
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

/**
 * Pantalla de juego. Vertical: pantallas arriba y botones abajo. Horizontal: pantallas una al lado de la otra
 * a pantalla completa y botones encima (posicion y tamano configurables en Ajustes).
 */
class GameActivity : AppCompatActivity() {
    private lateinit var prefs: Prefs
    private lateinit var gl: GLSurfaceView
    private lateinit var holder: FrameLayout
    private var controls: ControlsView? = null
    private val audio = AudioPlayer()

    @Volatile private var viewW = 1
    @Volatile private var viewH = 1
    @Volatile private var layoutDirty = true
    private var layoutArr = FloatArray(10)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        prefs = Prefs(this)
        requestedOrientation = when (prefs.orientation) {
            1 -> ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
            2 -> ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            else -> ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR
        }
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
                override fun onSurfaceCreated(unused: GL10?, c: EGLConfig?) { NativeBridge.engineGlInit() }
                override fun onSurfaceChanged(unused: GL10?, width: Int, height: Int) {
                    viewW = width; viewH = height; layoutDirty = true
                    NativeBridge.engineSetViewport(width, height)
                    GLES30.glViewport(0, 0, width, height)
                }
                override fun onDrawFrame(unused: GL10?) {
                    if (layoutDirty) {
                        layoutArr = NativeBridge.engineGetLayout(viewW.toFloat(), viewH.toFloat())
                        layoutDirty = false
                    }
                    GLES30.glDisable(GLES30.GL_SCISSOR_TEST)
                    GLES30.glClearColor(0.02f, 0.03f, 0.08f, 1f)
                    GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT)
                    if (!NativeBridge.engineIsRunning()) {          // sin ROM cargada: marcadores de posicion
                        fill(0, 0.10f, 0.25f, 0.55f)
                        fill(5, 0.10f, 0.45f, 0.30f)
                    }
                    NativeBridge.engineFrame()                      // ejecuta un fotograma y dibuja las dos pantallas
                }
            })
        }
        holder = FrameLayout(this)
        setContentView(holder)
        buildLayout()
        hideSystemBars()
    }

    private fun buildLayout() {
        val land = resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE
        (gl.parent as? ViewGroup)?.removeView(gl)
        controls?.let { (it.parent as? ViewGroup)?.removeView(it) }
        holder.removeAllViews()
        controls = if (prefs.showControls) ControlsView(this).apply {
            landscapeMode = land
            loadFrom(prefs)
            opacity = prefs.controlsOpacity
            onKeys = { mask -> NativeBridge.engineSetKeys(mask) }
            onSwap = {
                prefs.swapped = !prefs.swapped
                prefs.applyToEngine()
                layoutDirty = true
            }
        } else null
        val mp = ViewGroup.LayoutParams.MATCH_PARENT
        if (land) {
            holder.addView(gl, FrameLayout.LayoutParams(mp, mp))
            controls?.let { holder.addView(it, FrameLayout.LayoutParams(mp, mp)) }
        } else {
            val col = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
            col.addView(gl, LinearLayout.LayoutParams(mp, 0, 3f))
            controls?.let { col.addView(it, LinearLayout.LayoutParams(mp, 0, 2f)) }
            holder.addView(col, FrameLayout.LayoutParams(mp, mp))
        }
        layoutDirty = true
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        buildLayout()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemBars()
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

    override fun onPause() {
        super.onPause()
        audio.stop()
        gl.onPause()
        NativeBridge.engineSetKeys(0)
        NativeBridge.engineFlush()
    }

    override fun onResume() {
        super.onResume()
        gl.onResume()
        audio.start()
        hideSystemBars()
    }

    override fun onDestroy() {
        NativeBridge.engineFlush()
        super.onDestroy()
    }
}

package com.sonicrecomp.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.view.MotionEvent
import android.view.View
import kotlin.math.hypot

/** Los 7 grupos de controles y sus valores por defecto (x, y = fraccion de la vista; s = escala). */
object Ctl {
    const val DPAD = 0
    const val ABXY = 1
    const val BTN_L = 2
    const val BTN_R = 3
    const val BTN_SELECT = 4
    const val BTN_START = 5
    const val BTN_SWAP = 6
    const val COUNT = 7
    val NAMES = arrayOf("Cruceta", "Botones A / B / X / Y", "Botón L", "Botón R", "Select", "Start", "Intercambiar pantallas")
    private val PX = floatArrayOf(0.24f, 0.76f, 0.18f, 0.82f, 0.41f, 0.59f, 0.50f)
    private val PY = floatArrayOf(0.60f, 0.56f, 0.10f, 0.10f, 0.87f, 0.87f, 0.12f)
    private val PS = floatArrayOf(1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f)
    private val LX = floatArrayOf(0.13f, 0.87f, 0.07f, 0.93f, 0.42f, 0.58f, 0.50f)
    private val LY = floatArrayOf(0.66f, 0.62f, 0.14f, 0.14f, 0.90f, 0.90f, 0.12f)
    private val LS = floatArrayOf(0.62f, 0.62f, 0.62f, 0.62f, 0.70f, 0.70f, 0.70f)
    fun defX(land: Boolean, g: Int): Float = if (land) LX[g] else PX[g]
    fun defY(land: Boolean, g: Int): Float = if (land) LY[g] else PY[g]
    fun defS(land: Boolean, g: Int): Float = if (land) LS[g] else PS[g]
}

/**
 * Botones en pantalla con multitoque. Cada grupo tiene posicion y tamano propios (editables).
 * En horizontal se dibuja encima del juego: los toques fuera de un control pasan a la pantalla tactil.
 * Las mascaras de bits coinciden con nds::Key (core/keys.h).
 */
class ControlsView(ctx: Context) : View(ctx) {
    var onKeys: ((Int) -> Unit)? = null
    var onSwap: (() -> Unit)? = null
    var onSelect: ((Int, Float) -> Unit)? = null
    var opacity = 70
        set(v) { field = v.coerceIn(20, 100); invalidate() }
    var editMode = false
    var landscapeMode = false
    var selected = -1
        private set

    companion object {
        const val A = 1 shl 0; const val B = 1 shl 1; const val SELECT = 1 shl 2; const val START = 1 shl 3
        const val RIGHT = 1 shl 4; const val LEFT = 1 shl 5; const val UP = 1 shl 6; const val DOWN = 1 shl 7
        const val R = 1 shl 8; const val L = 1 shl 9; const val X = 1 shl 10; const val Y = 1 shl 11
    }

    private val gx = FloatArray(Ctl.COUNT)
    private val gy = FloatArray(Ctl.COUNT)
    private val gs = FloatArray(Ctl.COUNT)
    private var pressed = 0
    private var dragDx = 0f
    private var dragDy = 0f
    private val tmp = RectF()

    private val fill = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }
    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeWidth = 4f }
    private val text = Paint(Paint.ANTI_ALIAS_FLAG).apply { textAlign = Paint.Align.CENTER }
    private val selPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeWidth = 6f; color = 0xFFFFD21F.toInt() }

    init { resetDefaults() }

    fun loadFrom(p: Prefs) {
        for (g in 0 until Ctl.COUNT) { gx[g] = p.ctlX(landscapeMode, g); gy[g] = p.ctlY(landscapeMode, g); gs[g] = p.ctlS(landscapeMode, g) }
        invalidate()
    }
    fun saveTo(p: Prefs) { for (g in 0 until Ctl.COUNT) p.setCtl(landscapeMode, g, gx[g], gy[g], gs[g]) }
    fun resetDefaults() {
        for (g in 0 until Ctl.COUNT) { gx[g] = Ctl.defX(landscapeMode, g); gy[g] = Ctl.defY(landscapeMode, g); gs[g] = Ctl.defS(landscapeMode, g) }
        invalidate()
    }
    fun setSelectedScale(s: Float) { if (selected >= 0) { gs[selected] = s.coerceIn(0.3f, 2.0f); invalidate() } }

    private fun unit(): Float = minOf(width, height).toFloat()
    private fun cxOf(g: Int): Float = gx[g] * width
    private fun cyOf(g: Int): Float = gy[g] * height
    private fun dpadR(): Float = 0.25f * unit() * gs[Ctl.DPAD]
    private fun abDr(): Float = 0.25f * unit() * gs[Ctl.ABXY]

    private fun rectFor(g: Int, out: RectF) {
        val u = unit(); val s = gs[g]; val x = cxOf(g); val y = cyOf(g)
        when (g) {
            Ctl.DPAD -> { val r = dpadR(); out.set(x - r, y - r, x + r, y + r) }
            Ctl.ABXY -> { val e = abDr() * 1.2f; out.set(x - e, y - e, x + e, y + e) }
            Ctl.BTN_L, Ctl.BTN_R -> { val w = 0.36f * u * s; val h = 0.14f * u * s; out.set(x - w / 2, y - h / 2, x + w / 2, y + h / 2) }
            Ctl.BTN_SELECT, Ctl.BTN_START -> { val w = 0.16f * u * s; val h = 0.10f * u * s; out.set(x - w / 2, y - h / 2, x + w / 2, y + h / 2) }
            else -> { val w = 0.18f * u * s; val h = 0.16f * u * s; out.set(x - w / 2, y - h / 2, x + w / 2, y + h / 2) }
        }
    }

    private fun hitAny(x: Float, y: Float): Boolean {
        for (g in 0 until Ctl.COUNT) { rectFor(g, tmp); if (tmp.contains(x, y)) return true }
        return false
    }

    private fun maskAt(x: Float, y: Float): Int {
        var m = 0
        val dr = dpadR(); val ddx = x - cxOf(Ctl.DPAD); val ddy = y - cyOf(Ctl.DPAD)
        if (hypot(ddx, ddy) < dr * 1.5f) {
            val t = dr * 0.30f
            if (ddx > t) m = m or RIGHT
            if (ddx < -t) m = m or LEFT
            if (ddy > t) m = m or DOWN
            if (ddy < -t) m = m or UP
        }
        val ar = abDr(); val ax = cxOf(Ctl.ABXY); val ay = cyOf(Ctl.ABXY); val off = ar * 0.74f; val rad = ar * 0.42f * 1.2f
        if (hypot(x - (ax + off), y - ay) <= rad) m = m or A
        if (hypot(x - ax, y - (ay + off)) <= rad) m = m or B
        if (hypot(x - ax, y - (ay - off)) <= rad) m = m or X
        if (hypot(x - (ax - off), y - ay) <= rad) m = m or Y
        rectFor(Ctl.BTN_L, tmp); if (tmp.contains(x, y)) m = m or L
        rectFor(Ctl.BTN_R, tmp); if (tmp.contains(x, y)) m = m or R
        rectFor(Ctl.BTN_SELECT, tmp); if (tmp.contains(x, y)) m = m or SELECT
        rectFor(Ctl.BTN_START, tmp); if (tmp.contains(x, y)) m = m or START
        return m
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        if (editMode) return editTouch(e)
        val act = e.actionMasked
        if (act == MotionEvent.ACTION_DOWN && !hitAny(e.x, e.y)) return false      // fuera de los controles: pasa al juego
        if (act == MotionEvent.ACTION_DOWN || act == MotionEvent.ACTION_POINTER_DOWN) {
            val i = e.actionIndex
            rectFor(Ctl.BTN_SWAP, tmp)
            if (tmp.contains(e.getX(i), e.getY(i))) { onSwap?.invoke(); return true }
        }
        var mask = 0
        if (act != MotionEvent.ACTION_UP && act != MotionEvent.ACTION_CANCEL) {
            for (i in 0 until e.pointerCount) {
                if (act == MotionEvent.ACTION_POINTER_UP && i == e.actionIndex) continue
                mask = mask or maskAt(e.getX(i), e.getY(i))
            }
        }
        if (mask != pressed) { pressed = mask; onKeys?.invoke(mask); invalidate() }
        return true
    }

    private fun editTouch(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                var hit = -1
                for (g in Ctl.COUNT - 1 downTo 0) { rectFor(g, tmp); if (tmp.contains(e.x, e.y)) { hit = g; break } }
                selected = hit
                if (hit >= 0) { dragDx = e.x - cxOf(hit); dragDy = e.y - cyOf(hit); onSelect?.invoke(hit, gs[hit]) }
                invalidate()
            }
            MotionEvent.ACTION_MOVE -> {
                if (selected >= 0 && width > 0 && height > 0) {
                    gx[selected] = ((e.x - dragDx) / width).coerceIn(0.03f, 0.97f)
                    gy[selected] = ((e.y - dragDy) / height).coerceIn(0.03f, 0.97f)
                    invalidate()
                }
            }
        }
        return true
    }

    override fun onDraw(c: Canvas) {
        if (width == 0 || height == 0) return
        val a = opacity * 255 / 100
        fun idle() { fill.color = (a * 35 / 100 shl 24) or 0xFFFFFF; stroke.color = (a shl 24) or 0xFFFFFF; text.color = (a shl 24) or 0xFFFFFF }
        fun down() { fill.color = (a shl 24) or 0xFFFFFF; stroke.color = (a shl 24) or 0xFFFFFF; text.color = (a shl 24) or 0x000000 }

        // Cruceta
        val dr = dpadR(); val dcx = cxOf(Ctl.DPAD); val dcy = cyOf(Ctl.DPAD); val arm = dr * 0.34f
        idle()
        c.drawRoundRect(RectF(dcx - dr, dcy - arm, dcx + dr, dcy + arm), 14f, 14f, fill)
        c.drawRoundRect(RectF(dcx - dr, dcy - arm, dcx + dr, dcy + arm), 14f, 14f, stroke)
        c.drawRoundRect(RectF(dcx - arm, dcy - dr, dcx + arm, dcy + dr), 14f, 14f, fill)
        c.drawRoundRect(RectF(dcx - arm, dcy - dr, dcx + arm, dcy + dr), 14f, 14f, stroke)
        down()
        if (pressed and UP != 0) c.drawRoundRect(RectF(dcx - arm, dcy - dr, dcx + arm, dcy - arm), 14f, 14f, fill)
        if (pressed and DOWN != 0) c.drawRoundRect(RectF(dcx - arm, dcy + arm, dcx + arm, dcy + dr), 14f, 14f, fill)
        if (pressed and LEFT != 0) c.drawRoundRect(RectF(dcx - dr, dcy - arm, dcx - arm, dcy + arm), 14f, 14f, fill)
        if (pressed and RIGHT != 0) c.drawRoundRect(RectF(dcx + arm, dcy - arm, dcx + dr, dcy + arm), 14f, 14f, fill)

        // A / B / X / Y
        val ar = abDr(); val ax = cxOf(Ctl.ABXY); val ay = cyOf(Ctl.ABXY); val off = ar * 0.74f; val br = ar * 0.42f
        text.textSize = br * 0.9f
        val labels = arrayOf("A", "B", "X", "Y")
        val masks = intArrayOf(A, B, X, Y)
        val px = floatArrayOf(ax + off, ax, ax, ax - off)
        val py = floatArrayOf(ay, ay + off, ay - off, ay)
        for (i in 0 until 4) {
            if (pressed and masks[i] != 0) down() else idle()
            c.drawCircle(px[i], py[i], br, fill); c.drawCircle(px[i], py[i], br, stroke)
            c.drawText(labels[i], px[i], py[i] + text.textSize * 0.35f, text)
        }

        // L, R, Select, Start, intercambiar
        val rects = intArrayOf(Ctl.BTN_L, Ctl.BTN_R, Ctl.BTN_SELECT, Ctl.BTN_START, Ctl.BTN_SWAP)
        val names = arrayOf("L", "R", "SELECT", "START", "⇄")
        val rmask = intArrayOf(L, R, SELECT, START, 0)
        for (i in rects.indices) {
            rectFor(rects[i], tmp)
            if (rmask[i] != 0 && pressed and rmask[i] != 0) down() else idle()
            c.drawRoundRect(tmp, 20f, 20f, fill); c.drawRoundRect(tmp, 20f, 20f, stroke)
            text.textSize = tmp.height() * (if (i < 2) 0.42f else if (i == 4) 0.55f else 0.34f)
            c.drawText(names[i], tmp.centerX(), tmp.centerY() + text.textSize * 0.35f, text)
        }
        if (editMode && selected >= 0) { rectFor(selected, tmp); tmp.inset(-8f, -8f); c.drawRoundRect(tmp, 24f, 24f, selPaint) }
    }
}

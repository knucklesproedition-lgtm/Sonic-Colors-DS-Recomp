package com.sonicrecomp.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.view.MotionEvent
import android.view.View
import kotlin.math.hypot

/**
 * Botones en pantalla: cruceta, A/B/X/Y, L/R, Start/Select y boton de intercambiar pantallas.
 * Soporta multitoque. Las mascaras de bits coinciden con nds::Key (core/keys.h).
 */
class ControlsView(ctx: Context) : View(ctx) {
    var onKeys: ((Int) -> Unit)? = null
    var onSwap: (() -> Unit)? = null
    var opacity = 70   // 20..100
        set(v) { field = v.coerceIn(20, 100); invalidate() }

    companion object {
        const val A = 1 shl 0; const val B = 1 shl 1; const val SELECT = 1 shl 2; const val START = 1 shl 3
        const val RIGHT = 1 shl 4; const val LEFT = 1 shl 5; const val UP = 1 shl 6; const val DOWN = 1 shl 7
        const val R = 1 shl 8; const val L = 1 shl 9; const val X = 1 shl 10; const val Y = 1 shl 11
    }

    private class Btn(val label: String, val mask: Int, val round: Boolean) {
        val rect = RectF()
        fun hit(x: Float, y: Float): Boolean {
            if (!round) return rect.contains(x, y)
            val r = rect.width() / 2 * 1.2f   // un poco de margen para el dedo
            return hypot(x - rect.centerX(), y - rect.centerY()) <= r
        }
    }

    private val btns = listOf(
        Btn("A", A, true), Btn("B", B, true), Btn("X", X, true), Btn("Y", Y, true),
        Btn("L", L, false), Btn("R", R, false), Btn("SELECT", SELECT, false), Btn("START", START, false)
    )
    private val swapRect = RectF()
    private var dcx = 0f; private var dcy = 0f; private var dr = 0f   // cruceta
    private var pressed = 0

    private val fill = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }
    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeWidth = 4f }
    private val text = Paint(Paint.ANTI_ALIAS_FLAG).apply { textAlign = Paint.Align.CENTER }

    override fun onSizeChanged(w: Int, h: Int, ow: Int, oh: Int) {
        val fw = w.toFloat(); val fh = h.toFloat()
        dr = minOf(fw * 0.22f, fh * 0.30f); dcx = fw * 0.24f; dcy = fh * 0.60f
        val acx = fw * 0.76f; val acy = fh * 0.56f; val br = dr * 0.42f; val d = dr * 0.74f
        fun circle(b: Btn, cx: Float, cy: Float) = b.rect.set(cx - br, cy - br, cx + br, cy + br)
        circle(btns[0], acx + d, acy); circle(btns[1], acx, acy + d)   // A derecha, B abajo
        circle(btns[2], acx, acy - d); circle(btns[3], acx - d, acy)   // X arriba, Y izquierda
        btns[4].rect.set(fw * 0.03f, fh * 0.03f, fw * 0.33f, fh * 0.17f)   // L
        btns[5].rect.set(fw * 0.67f, fh * 0.03f, fw * 0.97f, fh * 0.17f)   // R
        btns[6].rect.set(fw * 0.34f, fh * 0.82f, fw * 0.48f, fh * 0.92f)   // Select
        btns[7].rect.set(fw * 0.52f, fh * 0.82f, fw * 0.66f, fh * 0.92f)   // Start
        swapRect.set(fw * 0.42f, fh * 0.04f, fw * 0.58f, fh * 0.20f)
        text.textSize = br * 0.9f
    }

    private fun maskAt(x: Float, y: Float): Int {
        var m = 0
        val dx = x - dcx; val dy = y - dcy
        val dist = hypot(dx, dy)
        if (dist < dr * 1.5f) {
            val t = dr * 0.30f
            if (dx > t) m = m or RIGHT
            if (dx < -t) m = m or LEFT
            if (dy > t) m = m or DOWN
            if (dy < -t) m = m or UP
        }
        for (b in btns) if (b.hit(x, y)) m = m or b.mask
        return m
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        val act = e.actionMasked
        if (act == MotionEvent.ACTION_DOWN || act == MotionEvent.ACTION_POINTER_DOWN) {
            val i = e.actionIndex
            if (swapRect.contains(e.getX(i), e.getY(i))) { onSwap?.invoke(); return true }
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

    override fun onDraw(c: Canvas) {
        val a = opacity * 255 / 100
        fun idle() { fill.color = (a * 35 / 100 shl 24) or 0xFFFFFF; stroke.color = (a shl 24) or 0xFFFFFF; text.color = (a shl 24) or 0xFFFFFF }
        fun down() { fill.color = (a shl 24) or 0xFFFFFF; stroke.color = (a shl 24) or 0xFFFFFF; text.color = (a shl 24) or 0x000000 }

        // Cruceta (cruz) con la direccion pulsada resaltada
        val arm = dr * 0.34f
        val cross = listOf(
            RectF(dcx - dr, dcy - arm, dcx + dr, dcy + arm), RectF(dcx - arm, dcy - dr, dcx + arm, dcy + dr))
        idle(); cross.forEach { c.drawRoundRect(it, 14f, 14f, fill); c.drawRoundRect(it, 14f, 14f, stroke) }
        down()
        if (pressed and UP != 0) c.drawRoundRect(RectF(dcx - arm, dcy - dr, dcx + arm, dcy - arm), 14f, 14f, fill)
        if (pressed and DOWN != 0) c.drawRoundRect(RectF(dcx - arm, dcy + arm, dcx + arm, dcy + dr), 14f, 14f, fill)
        if (pressed and LEFT != 0) c.drawRoundRect(RectF(dcx - dr, dcy - arm, dcx - arm, dcy + arm), 14f, 14f, fill)
        if (pressed and RIGHT != 0) c.drawRoundRect(RectF(dcx + arm, dcy - arm, dcx + dr, dcy + arm), 14f, 14f, fill)

        for (b in btns) {
            if (pressed and b.mask != 0) down() else idle()
            if (b.round) c.drawCircle(b.rect.centerX(), b.rect.centerY(), b.rect.width() / 2, fill)
            else c.drawRoundRect(b.rect, 20f, 20f, fill)
            if (b.round) c.drawCircle(b.rect.centerX(), b.rect.centerY(), b.rect.width() / 2, stroke)
            else c.drawRoundRect(b.rect, 20f, 20f, stroke)
            val ts = text.textSize
            if (!b.round) text.textSize = ts * 0.55f
            c.drawText(b.label, b.rect.centerX(), b.rect.centerY() + text.textSize * 0.35f, text)
            text.textSize = ts
        }
        // Boton de intercambiar pantallas
        idle(); c.drawRoundRect(swapRect, 20f, 20f, fill); c.drawRoundRect(swapRect, 20f, 20f, stroke)
        val ts = text.textSize; text.textSize = ts * 0.9f
        c.drawText("⇄", swapRect.centerX(), swapRect.centerY() + text.textSize * 0.35f, text)
        text.textSize = ts
    }
}

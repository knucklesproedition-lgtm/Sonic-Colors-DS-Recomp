package com.sonicrecomp.app

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack

/**
 * Reproduce el audio del motor (estereo 16 bits, 32728 Hz). Espera a llenar el buffer antes de empezar y ajusta la
 * velocidad de reproduccion de forma suave: si el emulador va justo de tiempo, el audio se ralentiza un poco en vez de cortarse.
 */
class AudioPlayer {
    @Volatile private var running = false
    private var thread: Thread? = null
    private var track: AudioTrack? = null

    fun start(bufferMs: Int) {
        if (running) return
        val rate = 32728
        val target = rate * bufferMs / 1000                       // fotogramas de audio que queremos tener de reserva
        val minBuf = AudioTrack.getMinBufferSize(rate, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT)
        val size = maxOf(minBuf * 2, target * 4)
        val t = try {
            AudioTrack.Builder()
                .setAudioAttributes(AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build())
                .setAudioFormat(AudioFormat.Builder()
                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                    .setSampleRate(rate)
                    .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                    .build())
                .setBufferSizeInBytes(size)
                .setTransferMode(AudioTrack.MODE_STREAM)
                .build()
        } catch (e: Exception) { return }                       // sin audio no es motivo para fallar
        track = t
        running = true
        thread = Thread {
            var waited = 0                                       // pre-buffer: esperar (max. 1,5 s) a tener la reserva
            while (running && NativeBridge.engineAudioAvailable() < target && waited < 1500) { Thread.sleep(10); waited += 10 }
            if (running) t.play()
            val buf = ShortArray(512 * 2)
            var lastAdj = 0L
            while (running) {
                val n = NativeBridge.engineReadAudio(buf, 512)
                if (n > 0) t.write(buf, 0, n * 2) else Thread.sleep(3)
                val now = System.nanoTime()
                if (now - lastAdj > 100_000_000L) {              // cada 100 ms: corregir la velocidad segun la reserva
                    lastAdj = now
                    val avail = NativeBridge.engineAudioAvailable()
                    val err = (avail - target).toFloat() / maxOf(target, 1)
                    val f = (1f + 0.06f * err).coerceIn(0.90f, 1.04f)
                    try { t.setPlaybackRate((rate * f).toInt()) } catch (_: Exception) {}
                }
            }
        }.apply { isDaemon = true; start() }
    }

    fun stop() {
        running = false
        try { thread?.join(400) } catch (_: Exception) {}
        thread = null
        track?.let { try { it.stop() } catch (_: Exception) {}; it.release() }
        track = null
    }
}

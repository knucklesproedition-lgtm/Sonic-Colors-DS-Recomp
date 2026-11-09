package com.sonicrecomp.app

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack

/** Reproduce el audio que genera el motor (estereo 16 bits, 32728 Hz) con un hilo que alimenta un AudioTrack. */
class AudioPlayer {
    @Volatile private var running = false
    private var thread: Thread? = null
    private var track: AudioTrack? = null

    fun start() {
        if (running) return
        val rate = 32728
        val minBuf = AudioTrack.getMinBufferSize(rate, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT)
        val size = maxOf(minBuf * 2, rate / 8 * 4)
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
        t.play()
        thread = Thread {
            val buf = ShortArray(512 * 2)
            while (running) {
                val n = NativeBridge.engineReadAudio(buf, 512)
                if (n > 0) t.write(buf, 0, n * 2) else Thread.sleep(3)
            }
        }.apply { isDaemon = true; start() }
    }

    fun stop() {
        running = false
        try { thread?.join(300) } catch (_: Exception) {}
        thread = null
        track?.let { try { it.stop() } catch (_: Exception) {}; it.release() }
        track = null
    }
}

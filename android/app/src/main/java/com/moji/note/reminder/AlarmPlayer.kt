package com.moji.note.reminder

import android.content.Context
import android.media.AudioAttributes
import android.media.MediaPlayer
import android.media.RingtoneManager
import android.net.Uri
import android.os.Build
import android.os.PowerManager
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import androidx.core.content.ContextCompat
import org.json.JSONObject
import java.io.File

/**
 * 闹钟铃声：优先使用「该事项的铃声」，其次使用设置里的默认铃声，最后回退系统闹钟铃声。
 * 循环播放并伴随震动，直到 [stop]。
 */
object AlarmPlayer {
    private var player: MediaPlayer? = null
    private var vibrator: Vibrator? = null

    /** 区分真正的闹钟与设置页试听，避免试听的 5 秒计时器误停正在响的闹钟。 */
    private var owner: String? = null

    val isPlaying: Boolean get() = player != null

    fun isPlayingBy(owner: String): Boolean = isPlaying && this.owner == owner

    @Synchronized
    fun start(
        context: Context,
        eventRingtone: String?,
        volumePercent: Int,
        owner: String = "alarm",
    ) {
        stop()
        this.owner = owner
        val app = context.applicationContext
        val config = runCatching {
            JSONObject(File(File(app.filesDir, "moji"), "config.json").readText(Charsets.UTF_8))
        }.getOrDefault(JSONObject())
        val volume = (if (volumePercent in 0..100) volumePercent else 100) / 100f
        val dataDir = File(app.filesDir, "moji").canonicalFile
        val defaults = listOf(
            eventRingtone?.takeIf { it.isNotBlank() },
            config.optString("ringtone", "").takeIf { it.isNotBlank() },
        )
        val fileUris = defaults.mapNotNull { relative ->
            runCatching {
                val file = File(dataDir, relative!!.replace('\\', '/')).canonicalFile
                if (!file.path.startsWith(dataDir.path + File.separator)) null
                else file.takeIf { it.exists() }?.let { Uri.fromFile(it) }
            }.getOrNull()
        }
        val fallback = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM)
            ?: RingtoneManager.getDefaultUri(RingtoneManager.TYPE_RINGTONE)

        val attrs = AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_ALARM)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .build()

        for (uri in (fileUris + listOfNotNull(fallback)).distinct()) {
            val candidate = MediaPlayer()
            val ok = runCatching {
                candidate.setAudioAttributes(attrs)
                candidate.setWakeMode(app, PowerManager.PARTIAL_WAKE_LOCK)
                candidate.setDataSource(app, uri)
                candidate.isLooping = true
                candidate.setVolume(volume, volume)
                candidate.prepare()
                candidate.start()
                candidate
            }.getOrNull()
            if (ok != null) {
                player = ok
                break
            }
            runCatching { candidate.release() }
        }
        startVibration(app)
    }

    fun setVolume(volumePercent: Int) {
        val v = volumePercent.coerceIn(0, 100) / 100f
        runCatching { player?.setVolume(v, v) }
    }

    /**
     * 停止铃声。传入 owner 时，若当前播放已被另一个 owner 接管则不做任何事，
     * 这样试听的计时器不会误停真正的闹钟。
     */
    @Synchronized
    fun stop(owner: String? = null) {
        if (owner != null && this.owner != null && this.owner != owner) return
        val current = player
        player = null
        this.owner = null
        // 先取出并清空引用，即使 stop 抛异常也保证 release 会执行，避免 wake lock 泄漏。
        runCatching { current?.stop() }
        runCatching { current?.release() }
        runCatching { vibrator?.cancel() }
        vibrator = null
    }

    private fun startVibration(context: Context) {
        val vib = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            context.getSystemService(VibratorManager::class.java)?.defaultVibrator
        } else {
            @Suppress("DEPRECATION")
            ContextCompat.getSystemService(context, Vibrator::class.java)
        } ?: return
        val pattern = longArrayOf(0, 800, 600, 800, 900)
        runCatching {
            vib.vibrate(VibrationEffect.createWaveform(pattern, 0))
            vibrator = vib
        }
    }
}

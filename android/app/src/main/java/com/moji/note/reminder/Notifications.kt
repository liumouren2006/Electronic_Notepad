package com.moji.note.reminder

import android.Manifest
import android.annotation.SuppressLint
import android.app.KeyguardManager
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.media.AudioAttributes
import android.media.RingtoneManager
import android.os.Build
import android.os.PowerManager
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import com.moji.note.MainActivity
import com.moji.note.R
import com.moji.note.data.MojiEvent
import com.moji.note.data.MojiTime

object Notifications {
    /** 静音渠道：只在锁屏/息屏、确定会弹全屏闹钟页时使用，铃声由 AlarmActivity 循环播放。 */
    const val CHANNEL_ID = "moji-reminders-v2"

    /** 有声备用渠道：设备解锁后系统不会启动全屏页，用通知自带闹钟铃声保证能听见。 */
    const val CHANNEL_AUDIBLE_ID = "moji-reminders-audible-v1"

    const val EXTRA_EVENT_ID = "extra_event_id"
    const val ACTION_STOP_RING = "com.moji.note.action.STOP_RING"

    fun createChannel(context: Context) {
        val manager = context.getSystemService(NotificationManager::class.java) ?: return
        val silent = NotificationChannel(
            CHANNEL_ID,
            context.getString(R.string.reminder_channel_name),
            NotificationManager.IMPORTANCE_HIGH,
        ).apply {
            description = context.getString(R.string.reminder_channel_desc)
            enableVibration(true)
            setSound(null, null)
            setShowBadge(true)
            lockscreenVisibility = Notification.VISIBILITY_PUBLIC
        }
        manager.createNotificationChannel(silent)

        val alarmUri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM)
            ?: RingtoneManager.getDefaultUri(RingtoneManager.TYPE_RINGTONE)
            ?: RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION)
        val audible = NotificationChannel(
            CHANNEL_AUDIBLE_ID,
            context.getString(R.string.reminder_channel_audible_name),
            NotificationManager.IMPORTANCE_HIGH,
        ).apply {
            description = context.getString(R.string.reminder_channel_audible_desc)
            enableVibration(true)
            if (alarmUri != null) {
                setSound(
                    alarmUri,
                    AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_ALARM)
                        .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                        .build(),
                )
            }
            setShowBadge(true)
            lockscreenVisibility = Notification.VISIBILITY_PUBLIC
        }
        manager.createNotificationChannel(audible)
    }

    fun canPost(context: Context): Boolean {
        if (!NotificationManagerCompat.from(context).areNotificationsEnabled()) return false
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) return true
        return ContextCompat.checkSelfPermission(
            context,
            Manifest.permission.POST_NOTIFICATIONS,
        ) == PackageManager.PERMISSION_GRANTED
    }

    /**
     * 通知是否真的能被用户感知。渠道被用户关成 IMPORTANCE_NONE 时 notify() 不会报错，
     * 但什么都不会显示，此时不应把提醒标记为已送达。
     */
    fun canDeliver(context: Context): Boolean {
        if (!canPost(context)) return false
        return if (willLaunchFullScreen(context)) {
            // 全屏页负责发声，静音渠道可用即可；静音渠道被关时还能退回有声渠道。
            channelUsable(context, CHANNEL_ID) || channelUsable(context, CHANNEL_AUDIBLE_ID)
        } else {
            // 不会弹全屏页，必须依赖有声渠道。
            channelUsable(context, CHANNEL_AUDIBLE_ID)
        }
    }

    /** 设备解锁且正在使用时，Android 通常不会启动 full-screen intent，需改用有声渠道。 */
    private fun channelUsable(context: Context, id: String): Boolean {
        val manager = context.getSystemService(NotificationManager::class.java) ?: return true
        return manager.getNotificationChannel(id)?.importance != NotificationManager.IMPORTANCE_NONE
    }

    private fun willLaunchFullScreen(context: Context): Boolean {
        if (Build.VERSION.SDK_INT >= 34) {
            val manager = context.getSystemService(NotificationManager::class.java)
            if (manager != null && !manager.canUseFullScreenIntent()) return false
        }
        val power = context.getSystemService(PowerManager::class.java)
        val keyguard = context.getSystemService(KeyguardManager::class.java)
        val interactive = power?.isInteractive ?: false
        val locked = keyguard?.isKeyguardLocked ?: false
        return locked || !interactive
    }

    @SuppressLint("MissingPermission")
    fun show(context: Context, event: MojiEvent, now: Long = MojiTime.nowSeconds()) {
        if (!canPost(context)) return
        val alarmIntent = Intent(context, AlarmActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
            putExtra(EXTRA_EVENT_ID, event.id)
        }
        val alarmPending = PendingIntent.getActivity(
            context,
            event.id.hashCode(),
            alarmIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        val detailIntent = Intent(context, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
            putExtra(EXTRA_EVENT_ID, event.id)
        }
        val detailPending = PendingIntent.getActivity(
            context,
            event.id.hashCode() xor 0x1A1A,
            detailIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        val stopIntent = Intent(context, ReminderReceiver::class.java).apply {
            action = ACTION_STOP_RING
            putExtra(EXTRA_EVENT_ID, event.id)
        }
        val stopPending = PendingIntent.getBroadcast(
            context,
            event.id.hashCode() xor 0x2B2B,
            stopIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )

        val dueText = if (event.due != null) {
            "截止：" + MojiTime.fullDateTime(event.due)
        } else {
            "无截止时间"
        }
        val preferred = if (willLaunchFullScreen(context)) CHANNEL_ID else CHANNEL_AUDIBLE_ID
        val fallback = if (preferred == CHANNEL_ID) CHANNEL_AUDIBLE_ID else CHANNEL_ID
        val channel = if (channelUsable(context, preferred)) preferred else fallback
        val builder = NotificationCompat.Builder(context, channel)
            .setSmallIcon(R.drawable.ic_notification)
            .setColor(0xFF4F6EF7.toInt())
            .setContentTitle(event.title)
            .setContentText(dueText)
            .setStyle(NotificationCompat.BigTextStyle().bigText(
                buildString {
                    append(dueText)
                    if (event.remindBefore > 0) append("（提前 ${event.remindBefore} 分钟提醒）")
                    if (event.note.isNotBlank()) append("\n").append(event.note.take(200))
                },
            ))
            .setPriority(NotificationCompat.PRIORITY_MAX)
            .setCategory(NotificationCompat.CATEGORY_ALARM)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .setShowWhen(true)
            .setWhen(System.currentTimeMillis())
            .setAutoCancel(true)
            .setOnlyAlertOnce(true)
            .setContentIntent(alarmPending)
            .setFullScreenIntent(alarmPending, true)
            .addAction(R.drawable.ic_notification, "停止铃声", stopPending)
            .addAction(R.drawable.ic_notification, "查看详情", detailPending)
        runCatching {
            NotificationManagerCompat.from(context).notify(event.id.hashCode(), builder.build())
        }
    }
}

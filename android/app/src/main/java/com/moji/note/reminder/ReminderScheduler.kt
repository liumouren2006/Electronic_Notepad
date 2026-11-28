package com.moji.note.reminder

import android.app.AlarmManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Build
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import com.moji.note.data.MojiEvent
import com.moji.note.data.MojiTime
import java.util.concurrent.TimeUnit

object ReminderScheduler {
    const val ACTION_REMIND = "com.moji.note.action.REMIND"
    private const val PREFS = "moji_scheduled_alarms"
    private const val KEY_IDS = "event_ids"

    fun scheduleScan(context: Context) {
        val request = PeriodicWorkRequestBuilder<ReminderWorker>(15, TimeUnit.MINUTES)
            .build()
        WorkManager.getInstance(context).enqueueUniquePeriodicWork(
            "moji-reminder-scan",
            ExistingPeriodicWorkPolicy.UPDATE,
            request,
        )
    }

    /** 为所有待办安排精确闹钟；已删除/已完成的旧闹钟会被取消。 */
    fun scheduleAll(context: Context, events: List<MojiEvent>) {
        val manager = context.getSystemService(AlarmManager::class.java) ?: return
        val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val previous = prefs.getStringSet(KEY_IDS, emptySet()).orEmpty()
        val now = MojiTime.nowSeconds()
        val current = mutableSetOf<String>()
        for (event in events) {
            if (event.done) continue
            val moment = event.effectiveRemindAt() ?: continue
            if (moment <= now) continue
            current.add(event.id)
            val pending = alarmIntent(context, event.id)
            try {
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S || manager.canScheduleExactAlarms()) {
                    manager.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, moment * 1000L, pending)
                } else {
                    manager.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, moment * 1000L, pending)
                }
            } catch (_: SecurityException) {
                manager.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, moment * 1000L, pending)
            }
        }
        for (oldId in previous) {
            if (oldId !in current) {
                manager.cancel(alarmIntent(context, oldId))
            }
        }
        prefs.edit().putStringSet(KEY_IDS, current).apply()
    }

    private fun alarmIntent(context: Context, eventId: String): PendingIntent {
        val intent = Intent(context, ReminderReceiver::class.java).apply {
            action = ACTION_REMIND
            putExtra(Notifications.EXTRA_EVENT_ID, eventId)
        }
        return PendingIntent.getBroadcast(
            context,
            eventId.hashCode(),
            intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
    }
}

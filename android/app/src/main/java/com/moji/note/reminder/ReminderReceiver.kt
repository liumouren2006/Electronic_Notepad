package com.moji.note.reminder

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import com.moji.note.MojiApp
import com.moji.note.data.MojiTime
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

class ReminderReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val eventId = intent.getStringExtra(Notifications.EXTRA_EVENT_ID) ?: return
        if (intent.action == Notifications.ACTION_STOP_RING) {
            // 通知上的「停止铃声」：无论闹钟页是否还在前台，都停掉声音并收起通知。
            AlarmPlayer.stop()
            runCatching {
                androidx.core.app.NotificationManagerCompat.from(context)
                    .cancel(eventId.hashCode())
            }
            return
        }
        val pendingResult = goAsync()
        val appContext = context.applicationContext
        CoroutineScope(Dispatchers.IO).launch {
            try {
                val repo = MojiApp.repository(appContext)
                // 闹钟可能在进程存活期间由 PC/同步目录改过数据，先读盘再判断。
                repo.load()
                val now = MojiTime.nowSeconds()
                val event = repo.find(eventId)
                if (event != null && event.needsReminder(now) &&
                    Notifications.canDeliver(appContext)
                ) {
                    Notifications.show(appContext, event, now)
                    repo.markReminded(listOf(event.id), now)
                }
                ReminderScheduler.scheduleAll(appContext, repo.all())
                ReminderScheduler.scheduleScan(appContext)
            } finally {
                pendingResult.finish()
            }
        }
    }
}

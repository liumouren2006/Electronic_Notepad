package com.moji.note

import android.app.Application
import android.content.Context
import com.moji.note.data.Repository
import com.moji.note.reminder.Notifications
import com.moji.note.reminder.ReminderScheduler
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch

class MojiApp : Application() {
    override fun onCreate() {
        super.onCreate()
        Notifications.createChannel(this)
        ReminderScheduler.scheduleScan(this)
        CoroutineScope(SupervisorJob() + Dispatchers.IO).launch {
            runCatching {
                val repo = repository(this@MojiApp)
                ReminderScheduler.scheduleAll(this@MojiApp, repo.all())
            }
        }
    }

    companion object {
        @Volatile
        private var instance: Repository? = null

        fun repository(context: Context): Repository =
            instance ?: synchronized(this) {
                instance ?: Repository(context.applicationContext).also { instance = it }
            }
    }
}

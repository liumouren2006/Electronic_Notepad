package com.moji.note.reminder

import android.content.Context
import androidx.work.CoroutineWorker
import androidx.work.WorkerParameters
import com.moji.note.MojiApp
import com.moji.note.data.MojiTime

class ReminderWorker(context: Context, params: WorkerParameters) : CoroutineWorker(context, params) {
    override suspend fun doWork(): Result {
        return try {
            val repo = MojiApp.repository(applicationContext)
            repo.load()
            val now = MojiTime.nowSeconds()
            val due = repo.dueReminders(now)
            if (due.isNotEmpty() && Notifications.canDeliver(applicationContext)) {
                for (event in due) Notifications.show(applicationContext, event, now)
                repo.markReminded(due.map { it.id }, now)
            }
            ReminderScheduler.scheduleAll(applicationContext, repo.all())
            Result.success()
        } catch (_: Exception) {
            Result.retry()
        }
    }
}

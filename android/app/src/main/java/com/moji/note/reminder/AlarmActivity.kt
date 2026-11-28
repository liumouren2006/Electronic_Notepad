package com.moji.note.reminder

import android.content.Intent
import android.media.AudioManager
import android.os.Build
import android.os.Bundle
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Alarm
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Snooze
import androidx.compose.material.icons.filled.VolumeOff
import androidx.compose.material.icons.filled.VolumeUp
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.moji.note.MainActivity
import com.moji.note.MojiApp
import com.moji.note.data.MojiEvent
import com.moji.note.data.MojiTime
import kotlinx.coroutines.delay

/**
 * 全屏闹钟页：锁屏也会点亮，铃声循环，提供「5 分钟后再提醒 / 静音 / 完成」。
 */
class AlarmActivity : ComponentActivity() {
    private var eventId: String? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge(
            statusBarStyle = androidx.activity.SystemBarStyle.dark(android.graphics.Color.TRANSPARENT),
            navigationBarStyle = androidx.activity.SystemBarStyle.dark(android.graphics.Color.TRANSPARENT),
        )
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            setShowWhenLocked(true)
            setTurnScreenOn(true)
        } else {
            @Suppress("DEPRECATION")
            window.addFlags(
                WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED or
                    WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON or
                    WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD,
            )
        }
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        volumeControlStream = AudioManager.STREAM_ALARM

        eventId = intent.getStringExtra(Notifications.EXTRA_EVENT_ID)
        val repo = MojiApp.repository(this)
        val event = eventId?.let { repo.find(it) }
        if (event == null) {
            finish()
            return
        }
        val volume = repo.loadConfig().optInt("volume", 100)
        AlarmPlayer.start(this, event.ringtone, volume, owner = "alarm")

        setContent {
            var muted by remember { mutableStateOf(false) }
            var now by remember { mutableLongStateOf(MojiTime.nowSeconds()) }
            LaunchedEffect(Unit) {
                while (true) {
                    delay(1_000)
                    now = MojiTime.nowSeconds()
                }
            }
            BackHandler { dismiss() }

            MaterialTheme(colorScheme = darkColorScheme(primary = Color(0xFF8FA6FF))) {
                AlarmScreen(
                    event = event,
                    now = now,
                    muted = muted,
                    onToggleMute = {
                        muted = !muted
                        if (muted) AlarmPlayer.stop("alarm") else {
                            val vol = repo.loadConfig().optInt("volume", 100)
                            AlarmPlayer.start(this@AlarmActivity, event.ringtone, vol, owner = "alarm")
                        }
                    },
                    onSnooze = {
                        snooze(event)
                        finish()
                    },
                    onDone = {
                        done(event)
                        finish()
                    },
                    onOpenDetail = {
                        cancelNotification()
                        startActivity(
                            Intent(this@AlarmActivity, MainActivity::class.java).apply {
                                flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
                                putExtra(Notifications.EXTRA_EVENT_ID, event.id)
                            },
                        )
                        finish()
                    },
                    onDismiss = { dismiss() },
                )
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        val newId = intent.getStringExtra(Notifications.EXTRA_EVENT_ID)
        if (newId != null && newId != eventId) {
            // 同一时刻又有一个闹钟触发：用新事项重建页面并换铃声。
            AlarmPlayer.stop("alarm")
            recreate()
        }
    }

    private fun dismiss() {
        AlarmPlayer.stop("alarm")
        cancelNotification()
        finish()
    }

    private fun cancelNotification() {
        eventId?.let { id ->
            runCatching {
                androidx.core.app.NotificationManagerCompat.from(this).cancel(id.hashCode())
            }
        }
    }

    private fun snooze(event: MojiEvent) {
        AlarmPlayer.stop("alarm")
        cancelNotification()
        val repo = MojiApp.repository(this)
        repo.setSnooze(event.id, MojiTime.nowSeconds() + 5 * 60L)
        ReminderScheduler.scheduleAll(this, repo.all())
        ReminderScheduler.scheduleScan(this)
    }

    private fun done(event: MojiEvent) {
        AlarmPlayer.stop("alarm")
        cancelNotification()
        val repo = MojiApp.repository(this)
        repo.setDone(event.id, true)
        ReminderScheduler.scheduleAll(this, repo.all())
    }

    override fun onDestroy() {
        AlarmPlayer.stop("alarm")
        super.onDestroy()
    }
}

@Composable
private fun AlarmScreen(
    event: MojiEvent,
    now: Long,
    muted: Boolean,
    onToggleMute: () -> Unit,
    onSnooze: () -> Unit,
    onDone: () -> Unit,
    onOpenDetail: () -> Unit,
    onDismiss: () -> Unit,
) {
    val bg = Brush.verticalGradient(
        listOf(Color(0xFF101733), Color(0xFF1B2450), Color(0xFF2A1F4D)),
    )
    Box(
        Modifier
            .fillMaxSize()
            .background(bg)
            .statusBarsPadding()
            .navigationBarsPadding(),
    ) {
        IconButton(
            onClick = onDismiss,
            modifier = Modifier
                .align(Alignment.TopEnd)
                .padding(8.dp),
        ) {
            Icon(Icons.Filled.Close, "关闭闹钟", tint = Color.White.copy(alpha = 0.75f))
        }

        Column(
            Modifier
                .fillMaxSize()
                .padding(horizontal = 24.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Spacer(Modifier.height(40.dp))
            Icon(
                Icons.Filled.Alarm,
                null,
                tint = Color(0xFFB7C6FF),
                modifier = Modifier.size(44.dp),
            )
            Spacer(Modifier.height(6.dp))
            Text(
                MojiTime.clock(now),
                fontSize = 64.sp,
                fontWeight = FontWeight.Bold,
                color = Color.White,
                letterSpacing = 1.sp,
            )
            Text(
                "周${MojiTime.weekdayCn(now)} · ${MojiTime.date(now)}",
                fontSize = 14.sp,
                color = Color.White.copy(alpha = 0.65f),
            )
            Spacer(Modifier.height(28.dp))
            Surface(
                shape = RoundedCornerShape(26.dp),
                color = Color.White.copy(alpha = 0.09f),
                modifier = Modifier.fillMaxWidth(),
            ) {
                Column(Modifier.padding(20.dp)) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        if (event.icon.isNotBlank()) {
                            Text(event.icon, fontSize = 26.sp)
                            Spacer(Modifier.size(8.dp))
                        }
                        Text(
                            event.title,
                            fontSize = 24.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = Color.White,
                            maxLines = 2,
                            overflow = TextOverflow.Ellipsis,
                        )
                    }
                    Spacer(Modifier.height(10.dp))
                    Text(
                        if (event.due != null) {
                            "截止 ${MojiTime.fullDateTime(event.due)}（${event.dueText(now)}）"
                        } else {
                            "无截止时间"
                        },
                        fontSize = 14.sp,
                        color = Color(0xFFFFB4A9),
                    )
                    if (event.note.isNotBlank()) {
                        Spacer(Modifier.height(12.dp))
                        Text(
                            event.note.take(400),
                            fontSize = 14.sp,
                            lineHeight = 21.sp,
                            color = Color.White.copy(alpha = 0.78f),
                            maxLines = 8,
                            overflow = TextOverflow.Ellipsis,
                        )
                    }
                }
            }
            Spacer(Modifier.weight(1f))

            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                OutlinedButton(
                    onClick = onToggleMute,
                    modifier = Modifier.weight(1f),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = Color.White),
                ) {
                    Icon(
                        if (muted) Icons.Filled.VolumeOff else Icons.Filled.VolumeUp,
                        null,
                        modifier = Modifier.size(18.dp),
                    )
                    Spacer(Modifier.size(6.dp))
                    Text(if (muted) "恢复铃声" else "静音")
                }
                OutlinedButton(
                    onClick = onSnooze,
                    modifier = Modifier.weight(1.3f),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = Color.White),
                ) {
                    Icon(Icons.Filled.Snooze, null, modifier = Modifier.size(18.dp))
                    Spacer(Modifier.size(6.dp))
                    Text("5 分钟后再提醒")
                }
            }
            Spacer(Modifier.height(10.dp))
            Button(
                onClick = onDone,
                modifier = Modifier.fillMaxWidth(),
                colors = ButtonDefaults.buttonColors(
                    containerColor = Color(0xFF7E97FF),
                    contentColor = Color(0xFF0D1330),
                ),
            ) {
                Icon(Icons.Filled.Check, null, modifier = Modifier.size(19.dp))
                Spacer(Modifier.size(6.dp))
                Text("完成并关闭", fontWeight = FontWeight.SemiBold)
            }
            Text(
                "查看详情并停止铃声",
                color = Color.White.copy(alpha = 0.75f),
                fontSize = 14.sp,
                modifier = Modifier
                    .padding(vertical = 16.dp)
                    .fillMaxWidth()
                    .clickable { onOpenDetail() },
                textAlign = TextAlign.Center,
            )
        }
    }
}

package com.moji.note.data

import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Date
import java.util.Locale
import java.util.TimeZone

/** 与 PC 端一致的时间工具：本地时区、无时区后缀的 ISO 文本。 */
object MojiTime {
    fun nowSeconds(): Long = System.currentTimeMillis() / 1000L

    private fun formatter(pattern: String) = SimpleDateFormat(pattern, Locale.US).apply {
        timeZone = TimeZone.getDefault()
    }

    fun iso(seconds: Long?): String =
        if (seconds == null) "" else formatter("yyyy-MM-dd'T'HH:mm:ss").format(Date(seconds * 1000L))

    fun isoNow(): String = iso(nowSeconds())

    fun date(seconds: Long): String = formatter("yyyy-MM-dd").format(Date(seconds * 1000L))

    fun clock(seconds: Long): String = formatter("HH:mm").format(Date(seconds * 1000L))

    /** 兼容 PC 端解析：yyyy-MM-dd[T ]HH:mm[:ss]、M-d、HH:mm、/ 分隔、结尾 Z。 */
    fun parse(raw: String?): Long? {
        var text = (raw ?: "").trim()
        if (text.isEmpty()) return null
        text = text.replace('/', '-')
        text = text.trimEnd('Z', 'z')
        var datePart = text
        var timePart = ""
        val t = text.indexOf('T')
        if (t >= 0) {
            datePart = text.substring(0, t)
            timePart = text.substring(t + 1)
        } else {
            val sp = text.indexOf(' ')
            if (sp >= 0) {
                datePart = text.substring(0, sp)
                timePart = text.substring(sp + 1)
            }
        }
        var y = 0
        var m = 0
        var d = 0
        var h = 0
        var mi = 0
        var s = 0
        var hasDate = false
        if (datePart.isNotEmpty()) {
            val parts = datePart.split('-')
            when (parts.size) {
                3 -> {
                    y = parts[0].toIntOrNull() ?: return null
                    m = parts[1].toIntOrNull() ?: return null
                    d = parts[2].toIntOrNull() ?: return null
                    hasDate = true
                }
                2 -> {
                    val now = Calendar.getInstance()
                    y = now.get(Calendar.YEAR)
                    m = parts[0].toIntOrNull() ?: return null
                    d = parts[1].toIntOrNull() ?: return null
                    hasDate = true
                }
                else -> return null
            }
        }
        if (timePart.isNotEmpty()) {
            val parts = timePart.split(':')
            if (parts.isEmpty() || parts.size > 3) return null
            h = parts[0].toIntOrNull() ?: return null
            if (parts.size > 1) mi = parts[1].toIntOrNull() ?: return null
            if (parts.size > 2) s = parts[2].toIntOrNull() ?: return null
        } else if (text.contains(':') && !hasDate) {
            val parts = text.split(':')
            if (parts.isEmpty() || parts.size > 3) return null
            h = parts[0].toIntOrNull() ?: return null
            if (parts.size > 1) mi = parts[1].toIntOrNull() ?: return null
            if (parts.size > 2) s = parts[2].toIntOrNull() ?: return null
        }
        if (!hasDate) {
            val now = Calendar.getInstance()
            y = now.get(Calendar.YEAR)
            m = now.get(Calendar.MONTH) + 1
            d = now.get(Calendar.DAY_OF_MONTH)
        }
        if (m < 1 || m > 12 || d < 1 || d > 31 || h !in 0..23 || mi !in 0..59 ||
            s !in 0..60 || y < 1970
        ) return null
        val cal = Calendar.getInstance().apply {
            clear()
            set(y, m - 1, d, h, mi, s)
            set(Calendar.MILLISECOND, 0)
        }
        return cal.timeInMillis / 1000L
    }

    fun startOfDay(seconds: Long): Long = Calendar.getInstance().apply {
        timeInMillis = seconds * 1000L
        set(Calendar.HOUR_OF_DAY, 0)
        set(Calendar.MINUTE, 0)
        set(Calendar.SECOND, 0)
        set(Calendar.MILLISECOND, 0)
    }.timeInMillis / 1000L

    fun sameDay(a: Long, b: Long): Boolean = date(a) == date(b)

    fun weekdayCn(seconds: Long): String {
        val names = arrayOf("日", "一", "二", "三", "四", "五", "六")
        val c = Calendar.getInstance().apply { timeInMillis = seconds * 1000L }
        return names[c.get(Calendar.DAY_OF_WEEK) - 1]
    }

    fun fullDateTime(seconds: Long): String =
        formatter("yyyy-MM-dd HH:mm").format(Date(seconds * 1000L))

    fun humanize(due: Long?, now: Long = nowSeconds()): String {
        if (due == null) return "无期限"
        val diff = (due - now).toDouble()
        if (diff < 0) {
            val over = -diff
            return when {
                over < 3600 -> "已逾期 ${maxOf(1, (over / 60).toInt())} 分钟"
                over < 86400 -> "已逾期 ${(over / 3600).toInt()} 小时"
                else -> "已逾期 ${(over / 86400).toInt()} 天"
            }
        }
        if (diff < 3600) return "${maxOf(1, (diff / 60).toInt())} 分钟后"
        if (sameDay(due, now)) return "今天 ${clock(due)}"
        if (sameDay(due, now + 86400)) return "明天 ${clock(due)}"
        if (diff < 7 * 86400) return "周${weekdayCn(due)} ${clock(due)}"
        return fullDateTime(due)
    }
}

fun guessMime(name: String): String {
    val dot = name.lastIndexOf('.')
    val ext = if (dot < 0) "" else name.substring(dot).lowercase(Locale.US)
    return when (ext) {
        ".png" -> "image/png"
        ".jpg", ".jpeg" -> "image/jpeg"
        ".gif" -> "image/gif"
        ".webp" -> "image/webp"
        ".bmp" -> "image/bmp"
        ".svg" -> "image/svg+xml"
        ".mp3" -> "audio/mpeg"
        ".wav" -> "audio/wav"
        ".ogg" -> "audio/ogg"
        ".m4a" -> "audio/mp4"
        ".flac" -> "audio/flac"
        ".txt" -> "text/plain"
        ".md" -> "text/markdown"
        ".pdf" -> "application/pdf"
        ".zip" -> "application/zip"
        else -> "application/octet-stream"
    }
}

data class Attachment(
    val path: String = "",
    val name: String = "",
    val mime: String = "",
    val size: Long = 0L,
    val addedAt: String = "",
) {
    val isImage: Boolean
        get() = mime.startsWith("image/") || guessMime(name).startsWith("image/")
    val isAudio: Boolean
        get() = mime.startsWith("audio/") || guessMime(name).startsWith("audio/")
}

data class MojiEvent(
    val id: String = "",
    val title: String = "",
    val note: String = "",
    val due: Long? = null,
    val priority: Int = 1,
    val done: Boolean = false,
    val doneAt: Long? = null,
    val tags: List<String> = emptyList(),
    val attachments: List<Attachment> = emptyList(),
    val createdAt: Long? = null,
    val updatedAt: Long? = null,
    val remindBefore: Int = 0,
    val systemNotify: Boolean = false,
    val lastRemindedAt: Long? = null,
    val color: String = "",
    val icon: String = "",
    val pageLayout: String = "",
    val ringtone: String = "",
    val snoozeUntil: Long? = null,
) {
    fun isOverdue(now: Long = MojiTime.nowSeconds()): Boolean =
        due != null && !done && due < now

    fun isDueOn(day: Long): Boolean = due != null && MojiTime.sameDay(due, day)

    fun isDueToday(now: Long = MojiTime.nowSeconds()): Boolean =
        due != null && MojiTime.sameDay(due, now)

    fun remindAt(): Long? = due?.let { it - remindBefore.coerceAtLeast(0) * 60L }

    /** 实际下一次响铃时刻：手动“5 分钟后再提醒”会把它推后。 */
    fun effectiveRemindAt(): Long? {
        val base = remindAt() ?: return snoozeUntil
        return if (snoozeUntil != null && snoozeUntil > base) snoozeUntil else base
    }

    fun needsReminder(now: Long = MojiTime.nowSeconds()): Boolean {
        if (done || due == null) return false
        val moment = effectiveRemindAt() ?: return false
        if (now < moment) return false
        if (lastRemindedAt != null && lastRemindedAt >= moment) return false
        return true
    }

    fun displayTitle(): String = if (done) "✓ $title" else title

    fun badge(now: Long = MojiTime.nowSeconds()): String = when {
        done -> "已完成"
        isOverdue(now) -> "逾期"
        isDueToday(now) -> "今天"
        else -> "P$priority"
    }

    fun dueText(now: Long = MojiTime.nowSeconds()): String = MojiTime.humanize(due, now)

    fun fingerprint(): String = "$title\u001f${createdAt ?: 0L}"

    fun searchText(): String =
        buildString {
            append(title).append('\n')
            append(note).append('\n')
            append(tags.joinToString(" "))
        }.lowercase(Locale.getDefault())
}

/** 按 PC 端不变量重排序并重编号：priority 从 1 连续递增。 */
fun reindexed(events: List<MojiEvent>): List<MojiEvent> =
    events.sortedWith(compareBy({ it.priority }, { it.createdAt ?: 0L }))
        .mapIndexed { index, event -> event.copy(priority = index + 1) }

fun randomHex8(): String {
    val digits = "0123456789abcdef"
    return buildString(8) {
        repeat(8) { append(digits[(Math.random() * 16).toInt().coerceIn(0, 15)]) }
    }
}

fun splitTags(text: String): List<String> {
    val normalized = text.replace('，', ',')
    val out = LinkedHashSet<String>()
    for (raw in normalized.split(',')) {
        for (token in raw.trim().split(Regex("\\s+"))) {
            var t = token.trim().trimStart('#')
            if (t.isNotEmpty()) out.add(t)
        }
    }
    return out.toList()
}

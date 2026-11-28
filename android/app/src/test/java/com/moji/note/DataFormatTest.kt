package com.moji.note

import com.moji.note.data.Attachment
import com.moji.note.data.Merger
import com.moji.note.data.MjnoteCodec
import com.moji.note.data.MojiEvent
import com.moji.note.data.MojiJson
import com.moji.note.data.MojiTime
import com.moji.note.data.TodoTxt
import com.moji.note.data.reindexed
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayInputStream
import java.util.zip.ZipInputStream

class DataFormatTest {

    private fun sampleEvent() = MojiEvent(
        id = "59535cd3",
        title = "提交课程设计报告",
        note = "## 清单\n- [x] 核心功能\n\n| 项 | 状态 |\n| --- | --- |\n| 核心 | 完成 |",
        due = 1_764_000_000L,
        priority = 2,
        done = false,
        tags = listOf("课程", "截止"),
        attachments = listOf(
            Attachment(
                path = "attachments/59535cd3/cover.png",
                name = "cover.png",
                mime = "image/png",
                size = 172_854,
                addedAt = "2026-11-23T13:00:00",
            ),
        ),
        createdAt = 1_763_900_000L,
        updatedAt = 1_763_900_100L,
        remindBefore = 60,
        systemNotify = true,
        lastRemindedAt = null,
        color = "#5b8def",
        icon = "📌",
        pageLayout = """{"text":{"x":40.0,"y":40.0,"z":10.0,"w":680.0},"images":[],"hidden":[],"ink":[]}""",
        ringtone = "ringtones/class.mp3",
        snoozeUntil = 1_764_000_120L,
    )

    @Test
    fun eventJsonRoundTripKeepsPcFields() {
        val original = sampleEvent()
        val json = MojiJson.eventToJson(original)
        assertEquals("59535cd3", json.getString("id"))
        assertEquals("", json.getString("done_at"))
        assertEquals(2, json.getInt("priority"))
        assertEquals(60, json.getInt("remind_before"))
        assertTrue(json.getBoolean("system_notify"))
        assertEquals("📌", json.getString("icon"))
        assertTrue(json.getString("page_layout").contains("\"ink\""))

        val restored = MojiJson.eventFromJson(JSONObject(json.toString()))
        assertNotNull(restored)
        assertEquals(original.title, restored!!.title)
        assertEquals(original.note, restored.note)
        assertEquals(original.due, restored.due)
        assertEquals(original.tags, restored.tags)
        assertEquals(original.attachments, restored.attachments)
        assertEquals(original.color, restored.color)
        assertEquals(original.icon, restored.icon)
        assertEquals(original.pageLayout, restored.pageLayout)
        assertEquals(original.ringtone, restored.ringtone)
        assertEquals(original.snoozeUntil, restored.snoozeUntil)
        assertNull(restored.doneAt)
    }

    @Test
    fun snoozePushesReminderWithoutRepeating() {
        val now = 1_763_900_000L
        val event = sampleEvent().copy(
            due = now + 3600,
            remindBefore = 0,
            lastRemindedAt = null,
            snoozeUntil = now + 3900,
        )
        // 稍后提醒之前不再响
        assertFalse(event.needsReminder(now + 100))
        // 稍后提醒到点
        assertTrue(event.needsReminder(now + 3901))
        // 已经响过之后，不会因为原来的到期时间再次重复
        val reminded = event.copy(lastRemindedAt = now + 3901)
        assertFalse(reminded.needsReminder(now + 4000))
        assertEquals(now + 3900, reminded.effectiveRemindAt())

        // 早于基础提醒时刻的 snooze 不应把提醒提前
        val earlier = event.copy(snoozeUntil = now + 100)
        assertEquals(now + 3600, earlier.effectiveRemindAt())
        assertFalse(earlier.needsReminder(now + 200))
    }

    @Test
    fun eventsFileMatchesPcEnvelopeAndRecoversTagsString() {
        val text = MojiJson.eventsFile(listOf(sampleEvent()))
        val root = JSONObject(text)
        assertEquals("moji-note", root.getString("app"))
        assertEquals(1, root.getInt("version"))
        assertEquals(1, root.getInt("count"))
        val parsed = MojiJson.parseEvents(text)
        assertEquals(1, parsed.size)

        val loose = JSONObject(
            """{"events":[{"title":"旧格式","tags":"#a，b","priority":0,"done":true}]}""",
        )
        val legacy = MojiJson.parseEvents(loose.toString()).single()
        assertEquals(listOf("a", "b"), legacy.tags)
        assertEquals(1, legacy.priority)
        assertNotNull(legacy.doneAt)
    }

    @Test
    fun timeParsingSupportsPcVariants() {
        assertEquals(MojiTime.parse("2026-11-23T18:00:00"), MojiTime.parse("2026-11-23 18:00"))
        assertEquals(MojiTime.parse("2026/11/23 18:00:00"), MojiTime.parse("2026-11-23T18:00:00Z"))
        assertNull(MojiTime.parse(""))
        assertNull(MojiTime.parse("not-a-date"))
        val seconds = MojiTime.parse("2026-11-23T18:00:00")!!
        assertEquals("2026-11-23T18:00:00", MojiTime.iso(seconds))
    }

    @Test
    fun todoTxtImportExportRoundTrip() {
        val events = listOf(
            sampleEvent().copy(done = false, priority = 1, tags = listOf("课程 设计")),
            sampleEvent().copy(
                id = "aaaa0001",
                title = "完成服务器巡检",
                done = true,
                doneAt = 1_764_000_000L,
                priority = 2,
                tags = listOf("运维"),
            ),
        )
        val text = TodoTxt.export(events)
        assertTrue(text.startsWith("(A)"))
        assertTrue(text.contains("x "))
        assertTrue(text.contains("due:"))
        assertTrue(text.contains("+课程_设计"))

        val parsed = TodoTxt.import(text)
        assertEquals(2, parsed.size)
        assertEquals("提交课程设计报告", parsed[0].title)
        assertEquals(1, parsed[0].priority)
        assertEquals(events[0].due, parsed[0].due)
        assertTrue(parsed[0].tags.contains("课程 设计"))
        assertTrue(parsed[1].done)
        assertEquals("完成服务器巡检", parsed[1].title)
        assertEquals(2, parsed[1].priority)
    }

    @Test
    fun mergeSkipsDuplicatesAndReassignsConflictingIds() {
        val existing = listOf(sampleEvent())
        val sameFingerprint = sampleEvent().copy(
            id = "ffffffff",
            title = "提交课程设计报告",
            createdAt = 1_763_900_000L,
        )
        val conflict = sampleEvent().copy(title = "另一条记录", createdAt = 1_763_900_500L)
        val fresh = sampleEvent().copy(
            id = "abcdabcd",
            title = "全新记录",
            createdAt = 1_763_901_000L,
        )
        val (merged, report) = Merger.importEvents(
            existing,
            listOf(sameFingerprint, conflict, fresh),
            merge = true,
        )
        assertEquals(1, report.added)
        assertEquals(2, report.skipped)
        assertEquals(2, merged.size)
        assertEquals(1, merged[0].priority)
        assertEquals(2, merged[1].priority)
        assertTrue(merged.any { it.title == "全新记录" })
    }

    @Test
    fun syncMergePrefersNewerUpdatedAt() {
        val local = listOf(sampleEvent())
        val remoteNewer = sampleEvent().copy(
            title = "远端新标题",
            updatedAt = (sampleEvent().updatedAt ?: 0) + 10,
            priority = 5,
        )
        val remoteOlderSameId = sampleEvent().copy(
            title = "远端旧标题",
            updatedAt = (sampleEvent().updatedAt ?: 0) - 10,
        )
        val remoteUnique = MojiEvent(
            id = "zzzzzzzz",
            title = "远端独有记录",
            createdAt = 1_763_800_000L,
            updatedAt = 1_763_800_000L,
        )
        val result = Merger.syncMerge(
            local,
            listOf(remoteNewer, remoteOlderSameId, remoteUnique),
        )
        assertEquals(2, result.pulled)
        assertEquals(1, result.keptLocal)
        assertEquals(2, result.events.size)
        assertEquals("远端新标题", result.events.first { it.id == sampleEvent().id }.title)
    }

    @Test
    fun mjnoteBundleUsesStoredEntriesAndRoundTripsAttachments() {
        val event = sampleEvent()
        val payload = byteArrayOf(1, 2, 3, 4, 5)
        val bundle = MjnoteCodec.encode(listOf(event)) { payload }
        ZipInputStream(ByteArrayInputStream(bundle)).use { zip ->
            val first = zip.nextEntry
            assertNotNull(first)
            assertEquals("manifest.json", first.name)
            assertEquals(0, first.method)
            zip.closeEntry()
        }
        val decoded = MjnoteCodec.decode(bundle)
        assertEquals(1, decoded.events.size)
        val restored = decoded.events.single()
        assertEquals(event.title, restored.title)
        assertEquals(1, restored.attachments.size)
        assertEquals("cover.png", restored.attachments[0].name)
        assertTrue(payload.contentEquals(decoded.payloads["${event.id}#0"]))
        assertTrue(restored.pageLayout.contains("\"text\""))
    }

    @Test
    fun reindexForcesSequentialPriorities() {
        val events = listOf(
            sampleEvent().copy(id = "a", priority = 9, createdAt = 30),
            sampleEvent().copy(id = "b", priority = 2, createdAt = 20),
            sampleEvent().copy(id = "c", priority = 2, createdAt = 10),
        )
        val sorted = reindexed(events)
        assertEquals(listOf("c", "b", "a"), sorted.map { it.id })
        assertEquals(listOf(1, 2, 3), sorted.map { it.priority })
    }
}

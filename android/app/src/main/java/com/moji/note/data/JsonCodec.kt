package com.moji.note.data

import org.json.JSONArray
import org.json.JSONObject

/**
 * events.json / manifest 的读写。
 * 字段名、空时间写法、默认值与 PC 端 model.cpp 保持一致。
 */
object MojiJson {

    fun attachmentToJson(a: Attachment): JSONObject = JSONObject().apply {
        put("path", a.path)
        put("name", a.name)
        put("mime", a.mime)
        put("size", a.size)
        put("added_at", a.addedAt)
    }

    fun attachmentFromJson(o: JSONObject?): Attachment? {
        if (o == null) return null
        val path = o.optString("path", "")
        if (path.isEmpty()) return null
        var name = o.optString("name", "")
        if (name.isEmpty()) name = path.substringAfterLast('/')
        var mime = o.optString("mime", "")
        if (mime.isEmpty()) mime = guessMime(name)
        return Attachment(
            path = path,
            name = name,
            mime = mime,
            size = o.optLong("size", 0L),
            addedAt = o.optString("added_at", ""),
        )
    }

    fun eventToJson(e: MojiEvent): JSONObject = JSONObject().apply {
        put("id", e.id)
        put("title", e.title)
        put("note", e.note)
        put("due", MojiTime.iso(e.due))
        put("priority", e.priority)
        put("done", e.done)
        put("done_at", MojiTime.iso(if (e.done) e.doneAt else null))
        put("tags", JSONArray(e.tags))
        put(
            "attachments",
            JSONArray().apply { e.attachments.forEach { put(attachmentToJson(it)) } },
        )
        put("created_at", MojiTime.iso(e.createdAt))
        put("updated_at", MojiTime.iso(e.updatedAt ?: e.createdAt))
        put("remind_before", e.remindBefore)
        put("system_notify", e.systemNotify)
        put("last_reminded_at", MojiTime.iso(e.lastRemindedAt))
        put("color", e.color)
        put("icon", e.icon)
        put("page_layout", e.pageLayout)
        put("ringtone", e.ringtone)
        put("snooze_until", MojiTime.iso(e.snoozeUntil))
    }

    fun eventFromJson(o: JSONObject?): MojiEvent? {
        if (o == null) return null
        val title = o.optString("title", "").trim()
        if (title.isEmpty()) return null
        val done = o.optBoolean("done", false)
        var doneAt = MojiTime.parse(o.optString("done_at", ""))
        if (done && doneAt == null) doneAt = MojiTime.nowSeconds()
        if (!done) doneAt = null
        val attachmentList = mutableListOf<Attachment>()
        val atts = o.optJSONArray("attachments")
        if (atts != null) {
            for (i in 0 until atts.length()) {
                attachmentFromJson(atts.optJSONObject(i))?.let(attachmentList::add)
            }
        }
        val tagList = mutableListOf<String>()
        when (val tagsValue = o.opt("tags")) {
            is JSONArray -> for (i in 0 until tagsValue.length()) {
                tagsValue.optString(i).takeIf { it.isNotEmpty() }?.let(tagList::add)
            }
            is String -> tagList.addAll(splitTags(tagsValue))
        }
        val created = MojiTime.parse(o.optString("created_at", "")) ?: MojiTime.nowSeconds()
        var id = o.optString("id", "")
        if (id.isEmpty()) id = randomHex8()
        return MojiEvent(
            id = id,
            title = title,
            note = o.optString("note", ""),
            due = MojiTime.parse(o.optString("due", "")),
            priority = o.optInt("priority", 1).coerceAtLeast(1),
            done = done,
            doneAt = doneAt,
            tags = tagList.distinct(),
            attachments = attachmentList,
            createdAt = created,
            updatedAt = MojiTime.parse(o.optString("updated_at", "")) ?: created,
            remindBefore = o.optInt("remind_before", 0).coerceAtLeast(0),
            systemNotify = o.optBoolean("system_notify", false),
            lastRemindedAt = MojiTime.parse(o.optString("last_reminded_at", "")),
            color = o.optString("color", ""),
            icon = o.optString("icon", ""),
            pageLayout = o.optString("page_layout", ""),
            ringtone = o.optString("ringtone", ""),
            snoozeUntil = MojiTime.parse(o.optString("snooze_until", "")),
        )
    }

    /** 生成 events.json 文本（2 空格缩进，UTF-8 由调用方负责）。 */
    fun eventsFile(events: List<MojiEvent>, updatedAt: Long = MojiTime.nowSeconds()): String {
        val root = JSONObject().apply {
            put("app", "moji-note")
            put("version", 1)
            put("updated_at", MojiTime.iso(updatedAt))
            put("count", events.size)
            put(
                "events",
                JSONArray().apply { events.forEach { put(eventToJson(it)) } },
            )
        }
        return root.toString(2)
    }

    /** 解析 events.json / 导出 JSON；支持 {events:[...]} 或裸数组。 */
    fun parseEvents(text: String): List<MojiEvent> {
        val trimmed = text.trim()
        val array: JSONArray = if (trimmed.startsWith("[")) {
            JSONArray(trimmed)
        } else {
            val root = JSONObject(trimmed)
            root.optJSONArray("events") ?: throw IllegalArgumentException("文件格式不正确：缺少 events 数组")
        }
        val out = ArrayList<MojiEvent>(array.length())
        for (i in 0 until array.length()) {
            runCatching { eventFromJson(array.optJSONObject(i)) }
                .getOrNull()
                ?.let(out::add)
        }
        return out
    }

    fun defaultConfig(): JSONObject = JSONObject().apply {
        put("version", 1)
        put("theme", "light")
        put("beep", true)
        put("remind_interval_sec", 30)
        put("keep_backups", 20)
        put("window", JSONObject().put("w", 1280).put("h", 820))
        put("ringtone", "")
        put("volume", 100)
        put("accent", "#4f6ef7")
        put("app_name", "墨记")
        put("app_subtitle", "电子记事本")
        put("brand_logo", "")
        put("brand_logo_transform", "")
    }

    /** 与 PC Store::load_config 一样，用文件中的键覆盖默认值。 */
    fun mergeConfig(base: JSONObject, overlay: JSONObject?): JSONObject {
        val out = JSONObject(base.toString())
        if (overlay != null) {
            val keys = overlay.keys()
            while (keys.hasNext()) {
                val key = keys.next()
                out.put(key, overlay.get(key))
            }
        }
        return out
    }
}

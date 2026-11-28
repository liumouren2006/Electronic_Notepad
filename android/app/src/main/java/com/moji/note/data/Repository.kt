package com.moji.note.data

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import org.json.JSONObject
import java.io.File
import java.text.SimpleDateFormat
import java.util.Locale

data class ExportPayload(val fileName: String, val mime: String, val bytes: ByteArray)

data class CloudMergeReport(
    val added: Int = 0,
    val updated: Int = 0,
    val keptLocal: Int = 0,
    val localOnly: Int = 0,
)

data class LibraryStats(
    val total: Int = 0,
    val pending: Int = 0,
    val done: Int = 0,
    val today: Int = 0,
    val overdue: Int = 0,
    val attachments: Int = 0,
) {
    val rate: Int get() = if (total == 0) 0 else ((done.toDouble() * 100.0 / total) + 0.5).toInt()
}

/**
 * 本地仓库。目录布局与 PC 端一致：
 * events.json / events.json.bak / backups/events-*.json / config.json / attachments/<id>/
 */
class Repository(context: Context) {
    private val resolver = context.applicationContext.contentResolver
    val dataDir = File(context.applicationContext.filesDir, "moji")
    private val eventsFile = File(dataDir, "events.json")
    private val backupFile = File(dataDir, "events.json.bak")
    private val configFile = File(dataDir, "config.json")
    private val backupsDir = File(dataDir, "backups")
    private val attachmentsDir = File(dataDir, "attachments")
    private val lock = Any()

    private var events: List<MojiEvent> = emptyList()
    private val undoStack = ArrayDeque<List<MojiEvent>>()
    private val redoStack = ArrayDeque<List<MojiEvent>>()
    var warnings: List<String> = emptyList()
        private set

    init {
        dataDir.mkdirs()
        backupsDir.mkdirs()
        attachmentsDir.mkdirs()
        load()
    }

    fun load(): List<MojiEvent> = synchronized(lock) {
        warnings = emptyList()
        val loaded = readEventsFile() ?: emptyList()
        events = reindexed(loaded)
        events
    }

    private fun readEventsFile(): List<MojiEvent>? {
        val warningsBuffer = mutableListOf<String>()
        if (eventsFile.exists()) {
            try {
                return MojiJson.parseEvents(eventsFile.readText(Charsets.UTF_8))
            } catch (e: Exception) {
                warningsBuffer.add("events.json 损坏：${e.message}")
            }
        }
        if (backupFile.exists()) {
            try {
                warningsBuffer.add("已从 events.json.bak 恢复")
                return MojiJson.parseEvents(backupFile.readText(Charsets.UTF_8))
            } catch (e: Exception) {
                warningsBuffer.add("events.json.bak 损坏：${e.message}")
            }
        }
        warnings = warningsBuffer
        return null
    }

    fun all(): List<MojiEvent> = synchronized(lock) { events.toList() }

    fun find(id: String): MojiEvent? = synchronized(lock) { events.firstOrNull { it.id == id } }

    fun tags(): Map<String, Int> = synchronized(lock) {
        val map = sortedMapOf<String, Int>()
        for (e in events) for (t in e.tags) map[t] = (map[t] ?: 0) + 1
        return map
    }

    fun stats(now: Long = MojiTime.nowSeconds()): LibraryStats = synchronized(lock) {
        LibraryStats(
            total = events.size,
            pending = events.count { !it.done },
            done = events.count { it.done },
            today = events.count { it.isDueOn(now) },
            overdue = events.count { it.isOverdue(now) },
            attachments = events.sumOf { it.attachments.size },
        )
    }

    fun overdue(now: Long = MojiTime.nowSeconds()): List<MojiEvent> =
        synchronized(lock) { events.filter { it.isOverdue(now) } }

    fun dueReminders(now: Long = MojiTime.nowSeconds()): List<MojiEvent> =
        synchronized(lock) { events.filter { it.needsReminder(now) } }

    fun markReminded(ids: Collection<String>, at: Long = MojiTime.nowSeconds()) =
        synchronized(lock) {
            if (ids.isEmpty()) return
            val idSet = ids.toSet()
            events = events.map {
                if (it.id in idSet) it.copy(lastRemindedAt = at) else it
            }
            persist(events)
        }

    fun add(
        title: String,
        note: String = "",
        due: Long? = null,
        tags: List<String> = emptyList(),
        position: Int? = null,
        remindBefore: Int = 0,
        color: String = "",
        icon: String = "",
        systemNotify: Boolean = false,
        ringtone: String = "",
    ): MojiEvent = synchronized(lock) {
        val clean = title.trim()
        require(clean.isNotEmpty()) { "标题不能为空" }
        pushUndo()
        val now = MojiTime.nowSeconds()
        val event = MojiEvent(
            id = randomHex8(),
            title = clean,
            note = note,
            due = due,
            tags = tags.distinct(),
            createdAt = now,
            updatedAt = now,
            remindBefore = remindBefore.coerceAtLeast(0),
            color = color,
            icon = icon,
            systemNotify = systemNotify,
            ringtone = ringtone,
        )
        val list = events.toMutableList()
        val index = if (position == null) list.size else (position - 1).coerceIn(0, list.size)
        list.add(index, event)
        events = reindexed(list)
        persist(events)
        events.first { it.id == event.id }
    }

    fun update(event: MojiEvent): MojiEvent = synchronized(lock) {
        val index = events.indexOfFirst { it.id == event.id }
        require(index >= 0) { "找不到事件：${event.id}" }
        val clean = event.title.trim()
        require(clean.isNotEmpty()) { "标题不能为空" }
        pushUndo()
        val now = MojiTime.nowSeconds()
        val merged = event.copy(
            title = clean,
            doneAt = if (event.done) event.doneAt ?: now else null,
            remindBefore = event.remindBefore.coerceAtLeast(0),
            // 截止时间改动后，旧的“稍后提醒”与已提醒标记应失效，否则新时间不会响。
            snoozeUntil = if (event.due == events[index].due) event.snoozeUntil else null,
            lastRemindedAt = if (event.due == events[index].due) event.lastRemindedAt else null,
            updatedAt = now,
        )
        val list = events.toMutableList()
        list[index] = merged
        events = list
        persist(events)
        merged
    }

    fun setDone(id: String, done: Boolean): MojiEvent? = synchronized(lock) {
        val index = events.indexOfFirst { it.id == id }
        if (index < 0) return null
        pushUndo()
        val now = MojiTime.nowSeconds()
        val list = events.toMutableList()
        val updated = list[index].copy(
            done = done,
            doneAt = if (done) now else null,
            updatedAt = now,
        )
        list[index] = updated
        events = list
        persist(events)
        updated
    }

    fun toggleDone(id: String): MojiEvent? {
        val e = find(id) ?: return null
        return setDone(id, !e.done)
    }

    fun remove(id: String): Boolean = synchronized(lock) {
        if (events.none { it.id == id }) return false
        pushUndo()
        events = reindexed(events.filter { it.id != id })
        persist(events)
        true
    }

    fun clearDone(): Int = synchronized(lock) {
        val remaining = events.filter { !it.done }
        val removed = events.size - remaining.size
        if (removed > 0) {
            pushUndo()
            events = reindexed(remaining)
            persist(events)
        }
        removed
    }

    fun duplicate(id: String): MojiEvent? = synchronized(lock) {
        val index = events.indexOfFirst { it.id == id }
        if (index < 0) return null
        pushUndo()
        val now = MojiTime.nowSeconds()
        val src = events[index]
        val copy = src.copy(
            id = randomHex8(),
            title = src.title + "（副本）",
            done = false,
            doneAt = null,
            createdAt = now,
            updatedAt = now,
            lastRemindedAt = null,
            snoozeUntil = null,
        )
        val list = events.toMutableList()
        list.add(index + 1, copy)
        events = reindexed(list)
        persist(events)
        copy
    }

    fun moveTo(id: String, position: Int): Boolean = synchronized(lock) {
        val index = events.indexOfFirst { it.id == id }
        if (index < 0) return false
        pushUndo()
        val list = events.toMutableList()
        val item = list.removeAt(index)
        val target = (position - 1).coerceIn(0, list.size)
        list.add(target, item)
        events = reindexed(list)
        persist(events)
        true
    }

    fun moveBy(id: String, delta: Int): Boolean {
        val index = events.indexOfFirst { it.id == id }
        if (index < 0) return false
        val target = (index + delta + 1).coerceIn(1, events.size)
        return moveTo(id, target)
    }

    fun updatePageLayout(id: String, layout: String): MojiEvent? = synchronized(lock) {
        val index = events.indexOfFirst { it.id == id }
        if (index < 0) return null
        val now = MojiTime.nowSeconds()
        val list = events.toMutableList()
        list[index] = list[index].copy(pageLayout = layout, updatedAt = now)
        events = list
        persist(events)
        list[index]
    }

    /**
     * 云同步：解码服务端 .mjnote，把远端较新/新增的记录合并到本地，并落盘远端附件。
     * 与 SyncManager 的 events.json 合并规则一致：同 id 取 updated_at 较新的一方，
     * 不同 id 但标题+创建时间相同视为同一条。
     */
    fun mergeCloudBundle(bytes: ByteArray): CloudMergeReport = synchronized(lock) {
        val decoded = MjnoteCodec.decode(bytes)
        val local = events
        val localById = local.associateBy { it.id }
        val localFps = local.mapTo(mutableSetOf()) { it.fingerprint() }
        val localIds = localById.keys
        val prepared = decoded.events.map { remote ->
            val current = localById[remote.id]
            val remoteTime = remote.updatedAt ?: remote.createdAt ?: 0L
            val localTime = current?.let { it.updatedAt ?: it.createdAt ?: 0L }
            val wins = when {
                current != null -> remoteTime > localTime!!
                remote.fingerprint() in localFps -> false
                else -> true
            }
            if (!wins) {
                remote.copy(attachments = emptyList())
            } else {
                val saved = mutableListOf<Attachment>()
                remote.attachments.forEachIndexed { index, attachment ->
                    val payload = decoded.payloads["${remote.id}#$index"] ?: return@forEachIndexed
                    saved.add(
                        saveAttachmentBytes(
                            remote.id,
                            attachment.name,
                            payload,
                            attachment.mime,
                        ),
                    )
                }
                remote.copy(attachments = saved)
            }
        }
        val result = Merger.syncMerge(local, prepared)
        val added = (result.events.size - local.size).coerceAtLeast(0)
        val updated = (result.pulled - added).coerceAtLeast(0)
        val remoteIds = prepared.mapTo(mutableSetOf()) { it.id }
        val localOnly = localIds.count { it !in remoteIds }
        if (result.events != local) {
            pushUndo()
            events = result.events
            persist(events)
        }
        CloudMergeReport(
            added = added,
            updated = updated,
            keptLocal = result.keptLocal,
            localOnly = localOnly,
        )
    }

    /** 应用云端删除墓碑：云端删除且本地没有更新过的事件从本地移除。 */
    fun applyCloudTombstones(tombstones: Map<String, Long>): Int = synchronized(lock) {
        if (tombstones.isEmpty()) return 0
        val kept = events.filter { event ->
            val deletedAt = tombstones[event.id]
            deletedAt == null || (event.updatedAt ?: event.createdAt ?: 0L) > deletedAt
        }
        val removed = events.size - kept.size
        if (removed > 0) {
            pushUndo()
            events = reindexed(kept)
            persist(events)
        }
        removed
    }

    fun setRingtone(id: String, relativePath: String): MojiEvent? = synchronized(lock) {
        val index = events.indexOfFirst { it.id == id }
        if (index < 0) return null
        val list = events.toMutableList()
        list[index] = list[index].copy(
            ringtone = relativePath,
            updatedAt = MojiTime.nowSeconds(),
        )
        events = list
        persist(events)
        list[index]
    }

    fun setSnooze(id: String, until: Long?): MojiEvent? = synchronized(lock) {
        val index = events.indexOfFirst { it.id == id }
        if (index < 0) return null
        val list = events.toMutableList()
        list[index] = list[index].copy(
            snoozeUntil = until,
            updatedAt = MojiTime.nowSeconds(),
        )
        events = list
        persist(events)
        list[index]
    }

    /** 把用户选择的音频复制到 moji/ringtones/，返回可写进事件的相对路径。 */
    fun saveRingtoneBytes(rawName: String, bytes: ByteArray): String = synchronized(lock) {
        val folder = File(dataDir, "ringtones")
        folder.mkdirs()
        val safe = sanitizeName(rawName).ifBlank { "ringtone.mp3" }
        val dot = safe.lastIndexOf('.')
        val stem = if (dot > 0) safe.substring(0, dot) else safe
        val ext = if (dot > 0) safe.substring(dot) else ".mp3"
        var target = File(folder, safe)
        var counter = 1
        while (target.exists()) {
            target = File(folder, "$stem-$counter$ext")
            counter++
        }
        target.writeBytes(bytes)
        "ringtones/${target.name}"
    }

    /** 品牌 Logo 保存到 moji/brand/，返回相对路径。 */
    fun saveBrandLogoBytes(rawName: String, bytes: ByteArray): String = synchronized(lock) {
        val folder = File(dataDir, "brand")
        folder.mkdirs()
        val safe = sanitizeName(rawName).ifBlank { "logo.png" }
        val ext = safe.substringAfterLast('.', "png").lowercase(Locale.US)
        // 只保留当前 Logo，避免更换格式后留下旧文件。
        folder.listFiles()?.forEach { it.delete() }
        val target = File(folder, "logo.$ext")
        target.writeBytes(bytes)
        "brand/${target.name}"
    }

    fun clearBrandLogo() = synchronized(lock) {
        File(dataDir, "brand").listFiles()?.forEach { it.delete() }
    }

    /** 删除尚未被事项引用的铃声副本（仅允许 ringtones/ 下的相对路径）。 */
    fun deleteRingtone(relativePath: String): Boolean = synchronized(lock) {
        if (!relativePath.startsWith("ringtones/")) return false
        runCatching { resolve(relativePath).delete() }.getOrDefault(false)
    }

    // ------------------------------------------------------------------ 导入导出

    fun importJson(text: String, merge: Boolean = true): ImportReport {
        val incoming = MojiJson.parseEvents(text)
        return importList(incoming, merge)
    }

    fun importTodoTxt(text: String, merge: Boolean = true): ImportReport =
        importList(TodoTxt.import(text), merge)

    private fun importList(incoming: List<MojiEvent>, merge: Boolean): ImportReport =
        synchronized(lock) {
            pushUndo()
            val (list, report) = Merger.importEvents(events, incoming, merge)
            events = list
            persist(events)
            report
        }

    fun importBundle(bytes: ByteArray, merge: Boolean = true): ImportReport = synchronized(lock) {
        val decoded = MjnoteCodec.decode(bytes)
        pushUndo()
        val base = if (merge) events.toMutableList() else mutableListOf()
        val ids = base.mapTo(mutableSetOf()) { it.id }
        val fingerprints = base.mapTo(mutableSetOf()) { it.fingerprint() }
        var added = 0
        var skipped = 0
        val now = MojiTime.nowSeconds()
        for (oldEvent in decoded.events) {
            if (oldEvent.title.isBlank()) {
                skipped++
                continue
            }
            val fp = oldEvent.fingerprint()
            if (merge && (oldEvent.id in ids || fp in fingerprints)) {
                skipped++
                continue
            }
            var event = oldEvent
            if (event.id.isEmpty() || event.id in ids) {
                var newId = randomHex8()
                while (newId in ids) newId = randomHex8()
                event = event.copy(id = newId)
            }
            val newAttachments = mutableListOf<Attachment>()
            oldEvent.attachments.forEachIndexed { index, attachment ->
                val payload = decoded.payloads["${oldEvent.id}#$index"] ?: return@forEachIndexed
                newAttachments.add(
                    saveAttachmentBytes(event.id, attachment.name, payload, attachment.mime),
                )
            }
            event = event.copy(
                attachments = newAttachments,
                doneAt = if (event.done) event.doneAt ?: now else null,
                updatedAt = event.updatedAt ?: event.createdAt ?: now,
            )
            base.add(event)
            ids.add(event.id)
            fingerprints.add(fp)
            added++
        }
        events = reindexed(base)
        persist(events)
        ImportReport(added, skipped, decoded.events.size)
    }

    fun importAuto(bytes: ByteArray, fileName: String, merge: Boolean = true): ImportReport {
        val lower = fileName.lowercase(Locale.US)
        return when {
            lower.endsWith(".mjnote") -> importBundle(bytes, merge)
            lower.endsWith(".txt") -> importTodoTxt(
                String(bytes, Charsets.UTF_8),
                merge,
            )
            lower.endsWith(".md") || lower.endsWith(".markdown") ->
                throw IllegalArgumentException("暂不支持 Markdown 反向导入，请使用 .json / .mjnote / .txt")
            else -> importJson(String(bytes, Charsets.UTF_8), merge)
        }
    }

    fun exportJson(): String = synchronized(lock) { MojiJson.eventsFile(events) }

    fun exportTodoTxt(): String = synchronized(lock) { TodoTxt.export(events) }

    fun exportMarkdown(): String = synchronized(lock) { MarkdownExport.export(events) }

    fun exportBundle(): ByteArray = synchronized(lock) {
        MjnoteCodec.encode(events) { attachment ->
            runCatching { readAttachmentBytes(attachment) }.getOrNull()
        }
    }

    fun buildExport(format: String): ExportPayload {
        val stamp = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(java.util.Date())
        return when (format) {
            "mjnote" -> ExportPayload(
                "moji-$stamp.mjnote",
                "application/octet-stream",
                exportBundle(),
            )
            "txt" -> ExportPayload("moji-$stamp.txt", "text/plain", exportTodoTxt().toByteArray(Charsets.UTF_8))
            "md" -> ExportPayload("moji-$stamp.md", "text/markdown", exportMarkdown().toByteArray(Charsets.UTF_8))
            else -> ExportPayload("moji-$stamp.json", "application/json", exportJson().toByteArray(Charsets.UTF_8))
        }
    }

    // ------------------------------------------------------------------ 同步

    fun replaceAll(merged: List<MojiEvent>) = synchronized(lock) {
        events = reindexed(merged)
        persist(events)
    }

    fun saveCurrent() = synchronized(lock) { persist(events) }

    // ------------------------------------------------------------------ 附件

    fun addAttachment(eventId: String, uri: Uri): Attachment = synchronized(lock) {
        val event = events.firstOrNull { it.id == eventId }
            ?: throw IllegalArgumentException("找不到事件：$eventId")
        var name = "file"
        var size = 0L
        resolver.query(uri, null, null, null, null)?.use { cursor ->
            if (cursor.moveToFirst()) {
                val nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                if (nameIndex >= 0) name = cursor.getString(nameIndex) ?: name
                val sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE)
                if (sizeIndex >= 0 && !cursor.isNull(sizeIndex)) size = cursor.getLong(sizeIndex)
            }
        }
        val bytes = resolver.openInputStream(uri)?.use { it.readBytes() }
            ?: throw IllegalArgumentException("无法读取附件")
        val mime = resolver.getType(uri) ?: guessMime(name)
        pushUndo()
        val attachment = saveAttachmentBytes(eventId, name, bytes, mime)
        val list = events.toMutableList()
        val index = list.indexOfFirst { it.id == eventId }
        val now = MojiTime.nowSeconds()
        list[index] = list[index].copy(
            attachments = list[index].attachments + attachment,
            updatedAt = now,
        )
        events = list
        persist(events)
        attachment
    }

    fun addAttachmentBytes(eventId: String, name: String, bytes: ByteArray, mime: String? = null): Attachment =
        synchronized(lock) {
            val index = events.indexOfFirst { it.id == eventId }
            require(index >= 0) { "找不到事件：$eventId" }
            pushUndo()
            val attachment = saveAttachmentBytes(eventId, name, bytes, mime)
            val list = events.toMutableList()
            list[index] = list[index].copy(
                attachments = list[index].attachments + attachment,
                updatedAt = MojiTime.nowSeconds(),
            )
            events = list
            persist(events)
            attachment
        }

    fun removeAttachment(eventId: String, attachmentIndex: Int): Boolean = synchronized(lock) {
        val index = events.indexOfFirst { it.id == eventId }
        if (index < 0) return false
        val current = events[index]
        if (attachmentIndex !in current.attachments.indices) return false
        pushUndo()
        val list = events.toMutableList()
        list[index] = current.copy(
            attachments = current.attachments.filterIndexed { i, _ -> i != attachmentIndex },
            updatedAt = MojiTime.nowSeconds(),
        )
        events = list
        persist(events)
        true
    }

    private fun saveAttachmentBytes(
        eventId: String,
        rawName: String,
        bytes: ByteArray,
        mime: String? = null,
    ): Attachment {
        val safeId = if (isSafeSegment(eventId)) eventId else "misc"
        val folder = File(attachmentsDir, safeId)
        folder.mkdirs()
        val safeName = sanitizeName(rawName)
        val dot = safeName.lastIndexOf('.')
        val stem = if (dot >= 0) safeName.substring(0, dot) else safeName
        val ext = if (dot >= 0) safeName.substring(dot) else ""
        var target = File(folder, safeName)
        var counter = 1
        while (target.exists()) {
            target = File(folder, "$stem-$counter$ext")
            counter++
        }
        target.writeBytes(bytes)
        val name = target.name
        return Attachment(
            path = "attachments/$safeId/$name",
            name = name,
            mime = (mime?.takeIf { it.isNotBlank() } ?: guessMime(name)),
            size = bytes.size.toLong(),
            addedAt = MojiTime.isoNow(),
        )
    }

    fun resolveAttachment(attachment: Attachment): File? {
        return try {
            resolve(attachment.path)
        } catch (_: Exception) {
            null
        }
    }

    fun readAttachmentBytes(attachment: Attachment): ByteArray {
        val file = resolve(attachment.path)
        return file.readBytes()
    }

    fun readAttachmentBytes(eventId: String, relativePath: String): ByteArray =
        resolve(relativePath).readBytes()

    fun resolve(relativePath: String): File {
        val root = dataDir.canonicalFile
        val file = File(root, relativePath.replace('\\', '/')).canonicalFile
        val prefix = root.path + File.separator
        if (file.path != root.path && !file.path.startsWith(prefix)) {
            throw IllegalArgumentException("非法路径")
        }
        return file
    }

    private fun isSafeSegment(segment: String): Boolean =
        segment.isNotEmpty() && segment.none { it == '/' || it == '\\' || it == ':' } &&
            segment != "." && segment != ".."

    private fun sanitizeName(raw: String): String {
        val base = raw.substringAfterLast('/').substringAfterLast('\\')
        val sb = StringBuilder()
        for (ch in base) {
            if (ch.code < 32 || "<>:\"/\\|?*".contains(ch)) sb.append('_') else sb.append(ch)
        }
        var out = sb.toString().trim().trimEnd('.')
        if (out.isEmpty()) out = "file"
        if (out.length > 120) out = out.substring(0, 120)
        return out
    }

    // ------------------------------------------------------------------ 撤销

    fun canUndo(): Boolean = undoStack.isNotEmpty()

    fun canRedo(): Boolean = redoStack.isNotEmpty()

    fun undo(): Boolean = synchronized(lock) {
        if (undoStack.isEmpty()) return false
        redoStack.addLast(events)
        events = undoStack.removeLast()
        persist(events)
        true
    }

    fun redo(): Boolean = synchronized(lock) {
        if (redoStack.isEmpty()) return false
        undoStack.addLast(events)
        events = redoStack.removeLast()
        persist(events)
        true
    }

    private fun pushUndo() {
        undoStack.addLast(events)
        while (undoStack.size > 60) undoStack.removeFirst()
        redoStack.clear()
    }

    // ------------------------------------------------------------------ 配置 / 备份

    fun loadConfig(): JSONObject = synchronized(lock) {
        val defaults = MojiJson.defaultConfig()
        if (!configFile.exists()) return defaults
        return try {
            MojiJson.mergeConfig(defaults, JSONObject(configFile.readText(Charsets.UTF_8)))
        } catch (_: Exception) {
            defaults
        }
    }

    fun saveConfig(values: JSONObject) = synchronized(lock) {
        val merged = MojiJson.mergeConfig(loadConfig(), values)
        writeAtomic(configFile, merged.toString(2).toByteArray(Charsets.UTF_8))
    }

    fun backupFiles(): List<File> =
        backupsDir.listFiles { f -> f.isFile && f.name.startsWith("events-") && f.name.endsWith(".json") }
            ?.sortedBy { it.name }
            ?: emptyList()

    fun dataPath(): String = dataDir.absolutePath

    private fun persist(list: List<MojiEvent>) {
        val text = MojiJson.eventsFile(list)
        val bytes = text.toByteArray(Charsets.UTF_8)
        writeAtomic(eventsFile, bytes)
        writeAtomic(backupFile, bytes)
        backupsDir.mkdirs()
        val stamp = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(java.util.Date())
        writeAtomic(File(backupsDir, "events-$stamp.json"), bytes)
        val keep = loadConfig().optInt("keep_backups", 20).coerceAtLeast(0)
        val files = backupFiles()
        val excess = files.size - keep
        if (excess > 0) files.take(excess).forEach { it.delete() }
    }

    private fun writeAtomic(file: File, bytes: ByteArray) {
        file.parentFile?.mkdirs()
        val temp = File(file.parentFile, file.name + ".tmp-" + randomHex8().take(4))
        temp.writeBytes(bytes)
        if (!temp.renameTo(file)) {
            file.delete()
            if (!temp.renameTo(file)) {
                file.writeBytes(bytes)
                temp.delete()
            }
        }
    }
}

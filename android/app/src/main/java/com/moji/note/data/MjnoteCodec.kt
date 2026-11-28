package com.moji.note.data

import org.json.JSONArray
import org.json.JSONObject
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.nio.charset.StandardCharsets
import java.util.zip.CRC32
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream
import java.util.zip.ZipOutputStream

data class BundleDecoded(
    val events: List<MojiEvent>,
    /** key = "eventId#attachmentIndex"，value = 附件字节。 */
    val payloads: Map<String, ByteArray>,
)

/**
 * .mjnote 打包格式（与 PC 端 zip.cpp 完全兼容）。
 * - manifest.json
 * - files/<event-id>/<index>_<name>
 * PC 端只实现 method 0（store），因此写出时也用 STORED；读取兼容 deflate。
 */
object MjnoteCodec {

    private fun safeZipName(raw: String): String {
        val sb = StringBuilder()
        for (ch in raw) {
            sb.append(
                if (ch == '\\' || ch == '/' || ch == ':' || ch == '*' || ch == '?' ||
                    ch == '"' || ch == '<' || ch == '>' || ch == '|' || ch.code < 32
                ) '_' else ch,
            )
        }
        var name = sb.toString().trimStart('.')
        if (name.isEmpty()) name = "file"
        return name
    }

    fun encode(events: List<MojiEvent>, openAttachment: (Attachment) -> ByteArray?): ByteArray {
        val entries = mutableListOf<Pair<String, ByteArray>>()
        val manifestEvents = JSONArray()
        for (event in events) {
            val item = MojiJson.eventToJson(event)
            val attJson = JSONArray()
            event.attachments.forEachIndexed { index, attachment ->
                val data = openAttachment(attachment) ?: return@forEachIndexed
                val member = "files/${event.id}/${index}_${safeZipName(attachment.name)}"
                val aj = MojiJson.attachmentToJson(attachment)
                aj.put("src", member)
                attJson.put(aj)
                entries.add(member to data)
            }
            item.put("attachments", attJson)
            manifestEvents.put(item)
        }
        val manifest = JSONObject().apply {
            put("app", "moji-note")
            put("version", 2)
            put("exported_at", MojiTime.isoNow())
            put("events", manifestEvents)
            put("count", manifestEvents.length())
        }
        val manifestBytes = manifest.toString(2).toByteArray(StandardCharsets.UTF_8)
        entries.add(0, "manifest.json" to manifestBytes)
        return zipStore(entries)
    }

    fun decode(bytes: ByteArray): BundleDecoded {
        val files = LinkedHashMap<String, ByteArray>()
        ZipInputStream(ByteArrayInputStream(bytes)).use { zis ->
            while (true) {
                val entry = zis.nextEntry ?: break
                if (!entry.isDirectory) files[entry.name] = zis.readBytes()
                zis.closeEntry()
            }
        }
        val manifestBytes = files["manifest.json"]
            ?: throw IllegalArgumentException("打包文件缺少 manifest.json")
        val root = JSONObject(String(manifestBytes, StandardCharsets.UTF_8))
        val array = root.optJSONArray("events")
            ?: throw IllegalArgumentException("清单缺少事件数组")
        val payloads = LinkedHashMap<String, ByteArray>()
        val events = mutableListOf<MojiEvent>()
        for (i in 0 until array.length()) {
            val item = array.optJSONObject(i) ?: continue
            val event = runCatching { MojiJson.eventFromJson(item) }.getOrNull() ?: continue
            val newAttachments = mutableListOf<Attachment>()
            val atts = item.optJSONArray("attachments")
            if (atts != null) {
                for (j in 0 until atts.length()) {
                    val aj = atts.optJSONObject(j) ?: continue
                    val src = aj.optString("src", "")
                    if (src.isEmpty()) continue
                    val data = files[src] ?: continue
                    var name = aj.optString("name", "")
                    if (name.isEmpty()) name = src.substringAfterLast('/')
                    val attachment = Attachment(
                        path = aj.optString("path", ""),
                        name = name,
                        mime = aj.optString("mime", "").ifEmpty { guessMime(name) },
                        size = data.size.toLong(),
                        addedAt = aj.optString("added_at", "").ifEmpty { MojiTime.isoNow() },
                    )
                    newAttachments.add(attachment)
                    payloads["${event.id}#${newAttachments.lastIndex}"] = data
                }
            }
            events.add(event.copy(attachments = newAttachments))
        }
        return BundleDecoded(events, payloads)
    }

    private fun zipStore(entries: List<Pair<String, ByteArray>>): ByteArray {
        val out = ByteArrayOutputStream()
        ZipOutputStream(out, StandardCharsets.UTF_8).use { zos ->
            for ((name, data) in entries) {
                val crc = CRC32().apply { update(data) }
                val entry = ZipEntry(name).apply {
                    method = ZipEntry.STORED
                    size = data.size.toLong()
                    compressedSize = data.size.toLong()
                    this.crc = crc.value
                    time = System.currentTimeMillis()
                }
                zos.putNextEntry(entry)
                zos.write(data)
                zos.closeEntry()
            }
        }
        return out.toByteArray()
    }
}

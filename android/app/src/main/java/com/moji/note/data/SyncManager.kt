package com.moji.note.data

import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import java.io.File

data class SyncOutcome(
    val remoteCount: Int,
    val mergedCount: Int,
    val pulled: Int,
    val keptLocal: Int,
    val attachmentsCopied: Int,
)

/**
 * 与 PC 端数据目录（或另一台 Android）双向同步。
 * 目录结构约定为 events.json + attachments/<id>/<file>，与 PC 完全一致。
 */
class SyncManager(context: Context, private val repository: Repository) {
    private val appContext = context.applicationContext

    fun folderName(uri: Uri): String =
        DocumentFile.fromTreeUri(appContext, uri)?.name ?: "已选择的目录"

    fun readRemoteEvents(uri: Uri): String? {
        val root = DocumentFile.fromTreeUri(appContext, uri) ?: return null
        val file = root.findFile("events.json") ?: return null
        return appContext.contentResolver.openInputStream(file.uri)
            ?.use { it.readBytes().toString(Charsets.UTF_8) }
    }

    fun pull(uri: Uri, merge: Boolean = true): SyncOutcome {
        val text = readRemoteEvents(uri)
            ?: throw IllegalStateException("同步目录中没有 events.json，请先在 PC 端保存数据或先执行推送")
        val remote = MojiJson.parseEvents(text)
        val local = repository.all()
        val result = if (merge) {
            Merger.syncMerge(local, remote)
        } else {
            SyncResult(reindexed(remote), remote.size, 0)
        }
        var copied = 0
        for (event in result.events) {
            for (attachment in event.attachments) {
                if (localAttachmentExists(attachment)) continue
                val bytes = readRemoteAttachment(uri, attachment.path) ?: continue
                if (writeLocalAttachment(attachment.path, bytes)) copied++
            }
        }
        repository.replaceAll(result.events)
        return SyncOutcome(
            remoteCount = remote.size,
            mergedCount = result.events.size,
            pulled = result.pulled,
            keptLocal = result.keptLocal,
            attachmentsCopied = copied,
        )
    }

    fun push(uri: Uri): SyncOutcome {
        val root = DocumentFile.fromTreeUri(appContext, uri)
            ?: throw IllegalStateException("无法访问同步目录")
        val events = repository.all()
        val old = root.findFile("events.json")
        if (old != null && old.length() > 0) {
            appContext.contentResolver.openInputStream(old.uri)?.use { input ->
                writeRemoteFile(uri, "events.json.bak", input.readBytes())
            }
        }
        writeRemoteFile(uri, "events.json", repository.exportJson().toByteArray(Charsets.UTF_8))
        var copied = 0
        for (event in events) {
            for (attachment in event.attachments) {
                val local = repository.resolveAttachment(attachment) ?: continue
                if (!local.exists()) continue
                val remoteFile = findRemote(uri, attachment.path, createParents = true)
                if (remoteFile != null && remoteFile.exists() && remoteFile.length() == local.length()) {
                    continue
                }
                if (writeRemoteFile(uri, attachment.path, local.readBytes())) copied++
            }
        }
        return SyncOutcome(
            remoteCount = events.size,
            mergedCount = events.size,
            pulled = 0,
            keptLocal = 0,
            attachmentsCopied = copied,
        )
    }

    fun twoWay(uri: Uri): SyncOutcome {
        val pulled = pull(uri, merge = true)
        val pushed = push(uri)
        return pulled.copy(attachmentsCopied = pulled.attachmentsCopied + pushed.attachmentsCopied)
    }

    private fun localAttachmentExists(attachment: Attachment): Boolean {
        val file = repository.resolveAttachment(attachment) ?: return false
        return file.exists() && file.length() > 0
    }

    private fun writeLocalAttachment(relativePath: String, bytes: ByteArray): Boolean = try {
        val file = repository.resolve(relativePath)
        file.parentFile?.mkdirs()
        file.writeBytes(bytes)
        true
    } catch (_: Exception) {
        false
    }

    private fun readRemoteAttachment(uri: Uri, relativePath: String): ByteArray? {
        val file = findRemote(uri, relativePath, createParents = false) ?: return null
        if (!file.exists()) return null
        return runCatching {
            appContext.contentResolver.openInputStream(file.uri)?.use { it.readBytes() }
        }.getOrNull()
    }

    private fun findRemote(uri: Uri, relativePath: String, createParents: Boolean): DocumentFile? {
        val root = DocumentFile.fromTreeUri(appContext, uri) ?: return null
        var current = root
        val segments = relativePath.replace('\\', '/').split('/').filter { it.isNotBlank() }
        for ((index, segment) in segments.withIndex()) {
            val isLeaf = index == segments.lastIndex
            val found = current.findFile(segment)
            if (found != null) {
                current = found
                continue
            }
            if (!createParents) return null
            current = if (isLeaf) {
                current.createFile(guessMime(segment), segment) ?: return null
            } else {
                current.createDirectory(segment) ?: return null
            }
        }
        return current
    }

    private fun writeRemoteFile(uri: Uri, relativePath: String, bytes: ByteArray): Boolean {
        val file = findRemote(uri, relativePath, createParents = true) ?: return false
        return runCatching {
            appContext.contentResolver.openOutputStream(file.uri, "wt")?.use { it.write(bytes) }
            true
        }.getOrDefault(false)
    }
}

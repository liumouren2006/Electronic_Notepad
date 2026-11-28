package com.moji.note.data

import org.json.JSONArray
import org.json.JSONObject
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.nio.charset.StandardCharsets

/** 云同步服务默认地址（Tailscale 内网）。可在登录界面修改。 */
const val DEFAULT_CLOUD_SERVER = "http://dsbot.taild07bbe.ts.net:8789"

class CloudException(val code: Int, message: String) : Exception(message)

data class CloudSession(
    val token: String,
    val email: String,
    val displayName: String,
    val server: String,
)

data class CloudMeta(
    val version: Int,
    val updatedAt: Long,
    val size: Long,
    val eventCount: Int,
    val attachmentCount: Int,
    val updated: Map<String, Long>,
    val tombstones: Map<String, Long> = emptyMap(),
)

data class CloudDiff(
    val remoteOnly: Int,
    val remoteNewer: Int,
    val localOnly: Int,
    val localNewer: Int,
    val attachmentDelta: Int,
    val remoteDeleted: Int = 0,
) {
    val remoteChanges: Int get() = remoteOnly + remoteNewer + remoteDeleted
    val localChanges: Int get() = localOnly + localNewer
}

/** 极简 HTTP 客户端，不引入 OkHttp；服务端返回 org.json。 */
object CloudApi {
    fun normalizeServer(raw: String): String {
        var s = raw.trim().ifEmpty { DEFAULT_CLOUD_SERVER }
        while (s.endsWith('/')) s = s.dropLast(1)
        if (!s.startsWith("http://") && !s.startsWith("https://")) s = "http://$s"
        return s
    }

    fun register(server: String, email: String, password: String, displayName: String): CloudSession {
        val body = JSONObject()
            .put("email", email.trim())
            .put("password", password)
            .put("display_name", displayName.trim())
        val response = request(
            normalizeServer(server), "/api/register", "POST", body.toString(), null, 20_000,
        )
        return parseSession(response, server)
    }

    fun login(server: String, email: String, password: String): CloudSession {
        val body = JSONObject().put("email", email.trim()).put("password", password)
        val response = request(
            normalizeServer(server), "/api/login", "POST", body.toString(), null, 20_000,
        )
        return parseSession(response, server)
    }

    fun logout(server: String, token: String) {
        runCatching {
            request(normalizeServer(server), "/api/logout", "POST", "{}", token, 10_000)
        }
    }

    fun meta(server: String, token: String): CloudMeta {
        val text = request(normalizeServer(server), "/api/sync/meta", "GET", null, token, 25_000)
        val root = JSONObject(text)
        val updated = mutableMapOf<String, Long>()
        val summary = root.optJSONArray("summary") ?: JSONArray()
        for (i in 0 until summary.length()) {
            val item = summary.optJSONObject(i) ?: continue
            val id = item.optString("id", "")
            if (id.isEmpty()) continue
            updated[id] = MojiTime.parse(item.optString("updated_at", "")) ?: 0L
        }
        val tombstones = mutableMapOf<String, Long>()
        root.optJSONObject("tombstones")?.let { obj ->
            val keys = obj.keys()
            while (keys.hasNext()) {
                val id = keys.next()
                if (id.isNotEmpty()) tombstones[id] = obj.optLong(id, 0L)
            }
        }
        return CloudMeta(
            version = root.optInt("version", 0),
            updatedAt = root.optLong("updated_at", 0L),
            size = root.optLong("size", 0L),
            eventCount = root.optInt("event_count", 0),
            attachmentCount = root.optInt("attachment_count", 0),
            updated = updated,
            tombstones = tombstones,
        )
    }

    fun download(server: String, token: String): ByteArray {
        val conn = open(normalizeServer(server), "/api/sync/download", "GET", token, 120_000)
        return try {
            val code = conn.responseCode
            if (code !in 200..299) {
                val message = errorMessage(conn, code)
                throw CloudException(code, message)
            }
            conn.inputStream.use { it.readBytes() }
        } finally {
            conn.disconnect()
        }
    }

    fun upload(
        server: String,
        token: String,
        data: ByteArray,
        baseVersion: Int,
        deletedIds: List<String> = emptyList(),
    ): CloudMeta {
        val conn = open(normalizeServer(server), "/api/sync/upload", "POST", token, 180_000)
        conn.setRequestProperty("Content-Type", "application/octet-stream")
        conn.setRequestProperty("X-Moji-Base-Version", baseVersion.toString())
        if (deletedIds.isNotEmpty()) {
            conn.setRequestProperty("X-Moji-Deleted", JSONArray(deletedIds).toString())
        }
        conn.doOutput = true
        conn.setFixedLengthStreamingMode(data.size)
        return try {
            conn.outputStream.use { it.write(data) }
            val code = conn.responseCode
            val text = if (code in 200..299) {
                conn.inputStream.bufferedReader(StandardCharsets.UTF_8).use { it.readText() }
            } else {
                conn.errorStream?.bufferedReader(StandardCharsets.UTF_8)?.use { it.readText() } ?: ""
            }
            if (code !in 200..299) {
                val message = runCatching { JSONObject(text).optString("error") }
                    .getOrNull()?.takeIf { it.isNotBlank() } ?: "上传失败（HTTP $code）"
                throw CloudException(code, message)
            }
            val root = JSONObject(text)
            val updated = mutableMapOf<String, Long>()
            val summary = root.optJSONArray("summary") ?: JSONArray()
            for (i in 0 until summary.length()) {
                val item = summary.optJSONObject(i) ?: continue
                val id = item.optString("id", "")
                if (id.isNotEmpty()) {
                    updated[id] = MojiTime.parse(item.optString("updated_at", "")) ?: 0L
                }
            }
            val tombstones = mutableMapOf<String, Long>()
            root.optJSONObject("tombstones")?.let { obj ->
                val keys = obj.keys()
                while (keys.hasNext()) {
                    val id = keys.next()
                    if (id.isNotEmpty()) tombstones[id] = obj.optLong(id, 0L)
                }
            }
            CloudMeta(
                version = root.optInt("version", 0),
                updatedAt = root.optLong("updated_at", 0L),
                size = root.optLong("size", 0L),
                eventCount = root.optInt("event_count", 0),
                attachmentCount = root.optInt("attachment_count", 0),
                updated = updated,
                tombstones = tombstones,
            )
        } finally {
            conn.disconnect()
        }
    }

    private fun parseSession(text: String, server: String): CloudSession {
        val root = JSONObject(text)
        val token = root.optString("token", "")
        if (token.isEmpty()) throw CloudException(0, "服务器没有返回登录令牌")
        val user = root.optJSONObject("user") ?: JSONObject()
        return CloudSession(
            token = token,
            email = user.optString("email", ""),
            displayName = user.optString("display_name", ""),
            server = normalizeServer(root.optString("server", server)),
        )
    }

    private fun request(
        server: String,
        path: String,
        method: String,
        json: String?,
        token: String?,
        timeoutMs: Int,
    ): String {
        val conn = open(server, path, method, token, timeoutMs)
        try {
            if (json != null) {
                conn.setRequestProperty("Content-Type", "application/json; charset=utf-8")
                conn.doOutput = true
                conn.outputStream.use { it.write(json.toByteArray(StandardCharsets.UTF_8)) }
            }
            val code = conn.responseCode
            val text = if (code in 200..299) {
                conn.inputStream.bufferedReader(StandardCharsets.UTF_8).use { it.readText() }
            } else {
                conn.errorStream?.bufferedReader(StandardCharsets.UTF_8)?.use { it.readText() } ?: ""
            }
            if (code !in 200..299) {
                val message = runCatching { JSONObject(text).optString("error") }
                    .getOrNull()?.takeIf { it.isNotBlank() }
                    ?: when (code) {
                        401 -> "邮箱或密码不正确"
                        409 -> "云端数据已被其他设备更新"
                        else -> "请求失败（HTTP $code）"
                    }
                throw CloudException(code, message)
            }
            return text
        } catch (e: CloudException) {
            throw e
        } catch (e: IOException) {
            throw CloudException(0, "无法连接服务器：${e.message ?: "网络错误"}")
        } finally {
            conn.disconnect()
        }
    }

    private fun open(server: String, path: String, method: String, token: String?, timeoutMs: Int): HttpURLConnection {
        val conn = (URL(server + path).openConnection() as HttpURLConnection).apply {
            requestMethod = method
            connectTimeout = 15_000
            readTimeout = timeoutMs
            setRequestProperty("User-Agent", "MoJi-Android/1.2.0")
            setRequestProperty("Accept", "application/json")
            if (!token.isNullOrBlank()) setRequestProperty("Authorization", "Bearer $token")
        }
        return conn
    }

    private fun errorMessage(conn: HttpURLConnection, code: Int): String {
        val text = conn.errorStream?.bufferedReader(StandardCharsets.UTF_8)?.use { it.readText() }
            ?: ""
        return runCatching { JSONObject(text).optString("error") }.getOrNull()
            ?.takeIf { it.isNotBlank() }
            ?: when (code) {
                404 -> "云端还没有数据"
                401 -> "登录已失效，请重新登录"
                else -> "请求失败（HTTP $code）"
            }
    }
}

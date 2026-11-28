package com.moji.note

import com.moji.note.data.CloudApi
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.util.UUID
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

/**
 * 真实服务端联调测试。默认跳过；需要先建立到 dsbot:8789 的本地隧道：
 *   ssh -N -L 18790:100.116.113.16:8789 root@47.96.119.77
 * 然后：
 *   MOJI_CLOUD_TEST=1 ./gradlew :app:testDebugUnitTest --tests '*CloudApiIntegrationTest'
 */
class CloudApiIntegrationTest {
    @Test
    fun registerUploadDownload() {
        assumeTrue(System.getenv("MOJI_CLOUD_TEST") == "1")
        val server = System.getenv("MOJI_CLOUD_SERVER") ?: "http://127.0.0.1:18790"
        val email = "android-${UUID.randomUUID().toString().take(8)}@test.local"
        val session = CloudApi.register(server, email, "android123", "Android 测试")
        try {
            val before = CloudApi.meta(server, session.token)
            assertEquals(0, before.version)

            val manifest = JSONObject()
                .put("app", "moji-note")
                .put("version", 2)
                .put(
                    "events",
                    JSONArray().put(
                        JSONObject()
                            .put("id", "android01")
                            .put("title", "Android 云同步测试")
                            .put("updated_at", "2026-11-28T21:00:00")
                            .put(
                                "attachments",
                                JSONArray().put(
                                    JSONObject()
                                        .put("src", "files/android01/0_note.txt")
                                        .put("name", "note.txt"),
                                ),
                            ),
                    ),
                )
            val out = ByteArrayOutputStream()
            ZipOutputStream(out).use { zip ->
                zip.putNextEntry(ZipEntry("manifest.json"))
                zip.write(manifest.toString().toByteArray(Charsets.UTF_8))
                zip.closeEntry()
                zip.putNextEntry(ZipEntry("files/android01/0_note.txt"))
                zip.write("hello android".toByteArray(Charsets.UTF_8))
                zip.closeEntry()
            }
            val uploaded = CloudApi.upload(server, session.token, out.toByteArray(), 0)
            assertEquals(1, uploaded.version)
            assertEquals(1, uploaded.eventCount)
            assertEquals(1, uploaded.attachmentCount)
            assertTrue(uploaded.updated.containsKey("android01"))

            val downloaded = CloudApi.download(server, session.token)
            assertTrue(downloaded.size > 100)
            assertEquals(1, CloudApi.meta(server, session.token).version)

            // 删除墓碑：上传空快照并声明删除 android01，服务端应记录墓碑。
            val empty = JSONObject()
                .put("app", "moji-note")
                .put("version", 2)
                .put("events", JSONArray())
            val out2 = ByteArrayOutputStream()
            ZipOutputStream(out2).use { zip ->
                zip.putNextEntry(ZipEntry("manifest.json"))
                zip.write(empty.toString().toByteArray(Charsets.UTF_8))
                zip.closeEntry()
            }
            val deleted = CloudApi.upload(
                server,
                session.token,
                out2.toByteArray(),
                1,
                listOf("android01"),
            )
            assertEquals(2, deleted.version)
            assertTrue(deleted.tombstones.containsKey("android01"))
        } finally {
            CloudApi.logout(server, session.token)
        }
    }
}

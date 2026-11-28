package com.moji.note.ui

import android.app.Application
import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.moji.note.MojiApp
import com.moji.note.data.CloudApi
import com.moji.note.data.CloudDiff
import com.moji.note.data.CloudException
import com.moji.note.data.CloudMergeReport
import com.moji.note.data.CloudMeta
import com.moji.note.data.DEFAULT_CLOUD_SERVER
import com.moji.note.data.LibraryStats
import com.moji.note.data.MojiEvent
import com.moji.note.data.MojiJson
import com.moji.note.data.MojiTime
import com.moji.note.data.Repository
import com.moji.note.data.SyncManager
import com.moji.note.data.SyncOutcome
import com.moji.note.reminder.ReminderScheduler
import com.moji.note.ui.theme.ThemeMode
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import kotlin.math.ceil

enum class TaskFilter(val label: String) {
    ALL("全部"),
    PENDING("待办"),
    TODAY("今天"),
    OVERDUE("逾期"),
    DONE("已完成"),
}

/** 新建事项时先暂存在 ViewModel 里的附件，旋转屏幕不会丢。 */
data class DraftAttachment(
    val name: String,
    val mime: String,
    val bytes: ByteArray,
)

data class UiSettings(
    val themeMode: ThemeMode = ThemeMode.SYSTEM,
    val accentHex: String = "#4f6ef7",
    val dynamicColor: Boolean = false,
    val syncUri: String? = null,
    val syncLabel: String = "未连接",
    val appName: String = "墨记",
    val appSubtitle: String = "电子记事本",
    val brandLogo: String = "",
    val brandTransform: String = "",
    val defaultRingtone: String = "",
    val volume: Int = 100,
    /** 同名 Logo 被覆盖时路径不变，需要一个非持久化的版本号让 StateFlow 真正发出更新。 */
    val brandRevision: Int = 0,
    val cloudServer: String = DEFAULT_CLOUD_SERVER,
    val cloudEmail: String = "",
    val cloudToken: String = "",
    val cloudLastVersion: Int = 0,
    val cloudLastSync: Long = 0L,
    val cloudSkipVersion: Int = 0,
)

data class CloudPrompt(
    val meta: CloudMeta,
    val diff: CloudDiff,
    val manual: Boolean,
    val startup: Boolean,
    val deleted: Set<String> = emptySet(),
)

class MojiViewModel(app: Application) : AndroidViewModel(app) {
    val repository: Repository = MojiApp.repository(app)
    private val syncManager = SyncManager(app, repository)
    private val prefs = app.getSharedPreferences("moji_ui", Context.MODE_PRIVATE)

    private val _events = MutableStateFlow(repository.all())
    val events: StateFlow<List<MojiEvent>> = _events.asStateFlow()

    private val _stats = MutableStateFlow(repository.stats())
    val stats: StateFlow<LibraryStats> = _stats.asStateFlow()

    private val _tags = MutableStateFlow(repository.tags())
    val tags: StateFlow<Map<String, Int>> = _tags.asStateFlow()

    private val _undo = MutableStateFlow(false)
    private val _redo = MutableStateFlow(false)
    val canUndo: StateFlow<Boolean> = _undo.asStateFlow()
    val canRedo: StateFlow<Boolean> = _redo.asStateFlow()

    private val _filter = MutableStateFlow(TaskFilter.ALL)
    val filter: StateFlow<TaskFilter> = _filter.asStateFlow()

    private val _query = MutableStateFlow("")
    val query: StateFlow<String> = _query.asStateFlow()

    private val _selectedTag = MutableStateFlow<String?>(null)
    val selectedTag: StateFlow<String?> = _selectedTag.asStateFlow()

    /** 每 30 秒推进一次，让「今天 / 逾期 / 剩余时间」不依赖用户操作也能刷新。 */
    private val _tick = MutableStateFlow(MojiTime.nowSeconds())

    private val _settings = MutableStateFlow(loadSettings())
    val settings: StateFlow<UiSettings> = _settings.asStateFlow()

    private val _messages = MutableSharedFlow<String>(extraBufferCapacity = 16)
    val messages = _messages.asSharedFlow()

    private val _busy = MutableStateFlow(false)
    val busy: StateFlow<Boolean> = _busy.asStateFlow()

    private val _cloudBusy = MutableStateFlow(false)
    val cloudBusy: StateFlow<Boolean> = _cloudBusy.asStateFlow()

    private val _cloudPrompt = MutableStateFlow<CloudPrompt?>(null)
    val cloudPrompt: StateFlow<CloudPrompt?> = _cloudPrompt.asStateFlow()

    /** 新建事项时的暂存附件；随 ViewModel 生命周期存活，旋转屏幕不丢。 */
    val draftAttachments = androidx.compose.runtime.mutableStateListOf<DraftAttachment>()

    fun clearDraftAttachments() {
        draftAttachments.clear()
    }

    val visibleEvents: StateFlow<List<MojiEvent>> =
        combine(_events, _filter, _query, _selectedTag, _tick) { list, filter, query, tag, now ->
            list.filter { event ->
                val matchesFilter = when (filter) {
                    TaskFilter.ALL -> true
                    TaskFilter.PENDING -> !event.done
                    // 与 PC 的 due_on() 一致：今天到期包含已完成事项。
                    TaskFilter.TODAY -> event.isDueToday(now)
                    TaskFilter.OVERDUE -> event.isOverdue(now)
                    TaskFilter.DONE -> event.done
                }
                val matchesQuery = query.isBlank() ||
                    event.searchText().contains(query.trim().lowercase())
                val matchesTag = tag == null || tag in event.tags
                matchesFilter && matchesQuery && matchesTag
            }
        }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), repository.all())

    init {
        publish()
        refreshFromDisk()
        viewModelScope.launch {
            while (true) {
                kotlinx.coroutines.delay(30_000)
                _tick.value = MojiTime.nowSeconds()
                _stats.value = repository.stats()
            }
        }
    }

    // ------------------------------------------------------------------ 查询

    fun setFilter(filter: TaskFilter) {
        _filter.value = filter
    }

    fun setQuery(query: String) {
        _query.value = query
    }

    fun setTag(tag: String?) {
        _selectedTag.value = tag
    }

    fun find(id: String): MojiEvent? = repository.find(id)

    fun publish() {
        _events.value = repository.all()
        _stats.value = repository.stats()
        _tags.value = repository.tags()
        _undo.value = repository.canUndo()
        _redo.value = repository.canRedo()
    }

    fun refreshFromDisk() {
        viewModelScope.launch(Dispatchers.IO) {
            repository.load()
            withContext(Dispatchers.Main) {
                publish()
                scheduleAlarms()
                repository.warnings.firstOrNull()?.let { showMessage(it) }
            }
        }
    }

    private fun mutate(block: () -> Unit) {
        runCatching(block)
            .onSuccess {
                publish()
                scheduleAlarms()
            }
            .onFailure { showMessage(it.message ?: "操作失败") }
    }

    private fun scheduleAlarms() {
        val events = repository.all()
        ReminderScheduler.scheduleAll(getApplication(), events)
    }

    private fun showMessage(text: String) {
        _messages.tryEmit(text)
    }

    // ------------------------------------------------------------------ 增删改

    fun saveEvent(
        id: String?,
        title: String,
        note: String,
        due: Long?,
        tags: List<String>,
        remindBefore: Int,
        color: String,
        icon: String,
        systemNotify: Boolean,
        position: Int? = null,
        pageLayout: String? = null,
    ): String? {
        var resultId: String? = null
        mutate {
            resultId = if (id.isNullOrEmpty()) {
                repository.add(
                    title = title,
                    note = note,
                    due = due,
                    tags = tags,
                    position = position,
                    remindBefore = remindBefore,
                    color = color,
                    icon = icon,
                    systemNotify = systemNotify,
                ).id
            } else {
                val current = repository.find(id) ?: throw IllegalArgumentException("记录不存在")
                repository.update(
                    current.copy(
                        title = title,
                        note = note,
                        due = due,
                        tags = tags,
                        remindBefore = remindBefore,
                        color = color,
                        icon = icon,
                        systemNotify = systemNotify,
                        pageLayout = pageLayout ?: current.pageLayout,
                    ),
                ).id
            }
        }
        return resultId
    }

    fun updatePageLayout(eventId: String, layout: String) {
        mutate { repository.updatePageLayout(eventId, layout) }
    }

    fun toggleDone(id: String) {
        mutate { repository.toggleDone(id) }
    }

    fun delete(id: String): Boolean {
        var ok = false
        mutate { ok = repository.remove(id) }
        if (ok) showMessage("已删除，可在撤销中恢复")
        return ok
    }

    fun duplicate(id: String) {
        mutate { repository.duplicate(id) }
        showMessage("已创建副本")
    }

    fun moveBy(id: String, delta: Int) {
        mutate { repository.moveBy(id, delta) }
    }

    fun moveTo(id: String, position: Int) {
        mutate { repository.moveTo(id, position) }
        showMessage("已移动到第 $position 位")
    }

    fun setDone(id: String, done: Boolean) {
        mutate { repository.setDone(id, done) }
    }

    fun clearDone() {
        var count = 0
        mutate { count = repository.clearDone() }
        if (count > 0) showMessage("已清理 $count 条已完成记录")
    }

    fun undo() {
        if (repository.undo()) {
            publish()
            scheduleAlarms()
            showMessage("已撤销")
        }
    }

    fun redo() {
        if (repository.redo()) {
            publish()
            scheduleAlarms()
            showMessage("已重做")
        }
    }

    // ------------------------------------------------------------------ 附件

    fun addAttachment(eventId: String, uri: Uri) {
        mutate { repository.addAttachment(eventId, uri) }
        showMessage("附件已添加")
    }

    fun addAttachmentBytes(eventId: String, name: String, bytes: ByteArray, mime: String?) {
        mutate { repository.addAttachmentBytes(eventId, name, bytes, mime) }
        showMessage("附件已添加")
    }

    fun removeAttachment(eventId: String, index: Int) {
        mutate { repository.removeAttachment(eventId, index) }
        showMessage("附件已移除")
    }

    // ------------------------------------------------------------------ 导入导出

    fun importFromUri(uri: Uri, merge: Boolean = true) {
        viewModelScope.launch(Dispatchers.IO) {
            _busy.value = true
            try {
                val resolver = getApplication<Application>().contentResolver
                val name = displayName(resolver, uri) ?: "import.json"
                val bytes = resolver.openInputStream(uri)?.use { it.readBytes() }
                    ?: throw IllegalStateException("无法读取文件")
                val report = repository.importAuto(bytes, name, merge)
                withContext(Dispatchers.Main) {
                    publish()
                    scheduleAlarms()
                }
                showMessage("${report.summary()}（$name）")
            } catch (e: Exception) {
                showMessage("导入失败：${e.message}")
            } finally {
                _busy.value = false
            }
        }
    }

    fun exportToUri(format: String, uri: Uri) {
        viewModelScope.launch(Dispatchers.IO) {
            _busy.value = true
            try {
                val payload = repository.buildExport(format)
                val resolver = getApplication<Application>().contentResolver
                resolver.openOutputStream(uri)?.use { it.write(payload.bytes) }
                    ?: throw IllegalStateException("无法写入文件")
                showMessage("已导出 ${payload.fileName}（${payload.bytes.size / 1024} KB）")
            } catch (e: Exception) {
                showMessage("导出失败：${e.message}")
            } finally {
                _busy.value = false
            }
        }
    }

    fun suggestedExportName(format: String): Pair<String, String> {
        val ext = when (format) {
            "mjnote" -> "mjnote"
            "txt" -> "txt"
            "md" -> "md"
            else -> "json"
        }
        val mime = when (format) {
            "mjnote" -> "application/octet-stream"
            "txt" -> "text/plain"
            "md" -> "text/markdown"
            else -> "application/json"
        }
        val stamp = java.text.SimpleDateFormat("yyyyMMdd-HHmmss", java.util.Locale.US)
            .format(java.util.Date())
        return "moji-$stamp.$ext" to mime
    }

    private fun displayName(resolver: ContentResolver, uri: Uri): String? =
        resolver.query(uri, null, null, null, null)?.use { cursor ->
            if (!cursor.moveToFirst()) return@use null
            val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            if (index >= 0) cursor.getString(index) else null
        }

    // ------------------------------------------------------------------ 同步

    fun setSyncFolder(uri: Uri) {
        val context = getApplication<Application>()
        runCatching {
            context.contentResolver.takePersistableUriPermission(
                uri,
                android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION or
                    android.content.Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
            )
        }
        prefs.edit().putString("sync_uri", uri.toString()).apply()
        _settings.value = _settings.value.copy(
            syncUri = uri.toString(),
            syncLabel = syncManager.folderName(uri),
        )
        showMessage("已连接同步目录：${syncManager.folderName(uri)}")
    }

    fun clearSyncFolder() {
        prefs.edit().remove("sync_uri").apply()
        _settings.value = _settings.value.copy(syncUri = null, syncLabel = "未连接")
    }

    fun sync(action: String) {
        val uriString = _settings.value.syncUri ?: run {
            showMessage("请先选择同步目录")
            return
        }
        val uri = Uri.parse(uriString)
        viewModelScope.launch(Dispatchers.IO) {
            _busy.value = true
            try {
                val outcome: SyncOutcome = when (action) {
                    "pull" -> syncManager.pull(uri, merge = true)
                    "push" -> syncManager.push(uri)
                    else -> syncManager.twoWay(uri)
                }
                repository.load()
                withContext(Dispatchers.Main) {
                    publish()
                    scheduleAlarms()
                }
                val actionText = when (action) {
                    "pull" -> "拉取"
                    "push" -> "推送"
                    else -> "双向同步"
                }
                showMessage(
                    "${actionText}完成：远端 ${outcome.remoteCount} 条，合并后 ${outcome.mergedCount} 条，" +
                        "采用远端 ${outcome.pulled} 条，复制附件 ${outcome.attachmentsCopied} 个",
                )
            } catch (e: Exception) {
                showMessage("同步失败：${e.message}")
            } finally {
                _busy.value = false
            }
        }
    }

    // ------------------------------------------------------------------ 云同步

    private fun cloudSavePrefs(
        server: String,
        email: String,
        token: String,
        lastVersion: Int = 0,
        lastSync: Long = 0L,
        skipVersion: Int = 0,
    ) {
        prefs.edit()
            .putString("cloud_server", server)
            .putString("cloud_email", email)
            .putString("cloud_token", token)
            .putInt("cloud_last_version", lastVersion)
            .putLong("cloud_last_sync", lastSync)
            .putInt("cloud_skip_version", skipVersion)
            .remove("cloud_last_ids")
            .apply()
        _settings.value = _settings.value.copy(
            cloudServer = server,
            cloudEmail = email,
            cloudToken = token,
            cloudLastVersion = lastVersion,
            cloudLastSync = lastSync,
            cloudSkipVersion = skipVersion,
        )
    }

    private fun cloudErrorMessage(e: Exception): String =
        when (e) {
            is CloudException -> if (e.code == 0) e.message ?: "网络错误" else e.message ?: "请求失败"
            else -> e.message ?: "未知错误"
        }

    fun cloudLogin(server: String, email: String, password: String) {
        if (_cloudBusy.value) return
        if (email.isBlank() || password.isEmpty()) {
            showMessage("请填写邮箱和密码")
            return
        }
        _cloudBusy.value = true
        viewModelScope.launch(Dispatchers.IO) {
            try {
                val session = CloudApi.login(server, email, password)
                withContext(Dispatchers.Main) {
                    cloudSavePrefs(session.server, session.email, session.token)
                    _cloudBusy.value = false
                    showMessage("已登录 ${session.email}，正在检查云端…")
                    cloudCheck(manual = true, startup = false)
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    _cloudBusy.value = false
                    showMessage("登录失败：${cloudErrorMessage(e)}")
                }
            }
        }
    }

    fun cloudRegister(server: String, email: String, password: String, displayName: String) {
        if (_cloudBusy.value) return
        if (email.isBlank() || password.length < 8) {
            showMessage("请填写邮箱，密码至少 8 位")
            return
        }
        _cloudBusy.value = true
        viewModelScope.launch(Dispatchers.IO) {
            try {
                val session = CloudApi.register(server, email, password, displayName)
                withContext(Dispatchers.Main) {
                    cloudSavePrefs(session.server, session.email, session.token)
                    _cloudBusy.value = false
                    showMessage("注册成功，已登录 ${session.email}")
                    cloudCheck(manual = true, startup = false)
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    _cloudBusy.value = false
                    showMessage("注册失败：${cloudErrorMessage(e)}")
                }
            }
        }
    }

    fun cloudLogout() {
        val current = _settings.value
        if (current.cloudToken.isNotBlank()) {
            val server = current.cloudServer
            val token = current.cloudToken
            viewModelScope.launch(Dispatchers.IO) { CloudApi.logout(server, token) }
        }
        prefs.edit()
            .remove("cloud_email")
            .remove("cloud_token")
            .putInt("cloud_last_version", 0)
            .putLong("cloud_last_sync", 0L)
            .putInt("cloud_skip_version", 0)
            .remove("cloud_last_ids")
            .apply()
        _settings.value = current.copy(
            cloudEmail = "",
            cloudToken = "",
            cloudLastVersion = 0,
            cloudLastSync = 0L,
            cloudSkipVersion = 0,
        )
        _cloudPrompt.value = null
        _cloudBusy.value = false
        showMessage("已退出云同步，本地数据不受影响")
    }

    fun cloudStartupCheck() {
        if (_settings.value.cloudToken.isBlank()) return
        cloudCheck(manual = false, startup = true)
    }

    fun cloudManualSync() {
        if (_settings.value.cloudToken.isBlank()) {
            showMessage("请先在「云同步」中登录")
            return
        }
        cloudCheck(manual = true, startup = false)
    }

    private fun cloudDiff(local: List<MojiEvent>, meta: CloudMeta): CloudDiff {
        val localMap = local.associate { it.id to (it.updatedAt ?: it.createdAt ?: 0L) }
        val localAttachments = local.sumOf { it.attachments.size }
        var remoteOnly = 0
        var remoteNewer = 0
        for ((id, updated) in meta.updated) {
            val localTime = localMap[id]
            if (localTime == null) {
                remoteOnly++
            } else if (updated > localTime) {
                remoteNewer++
            }
        }
        var localOnly = 0
        var localNewer = 0
        for ((id, localTime) in localMap) {
            val remoteTime = meta.updated[id]
            if (remoteTime == null) {
                localOnly++
            } else if (localTime > remoteTime) {
                localNewer++
            }
        }
        var remoteDeleted = 0
        for ((id, deletedAt) in meta.tombstones) {
            val localTime = localMap[id]
            if (localTime != null && localTime <= deletedAt) remoteDeleted++
        }
        return CloudDiff(
            remoteOnly = remoteOnly,
            remoteNewer = remoteNewer,
            localOnly = localOnly,
            localNewer = localNewer,
            attachmentDelta = kotlin.math.abs(meta.attachmentCount - localAttachments),
            remoteDeleted = remoteDeleted,
        )
    }

    private fun cloudCheck(manual: Boolean, startup: Boolean) {
        val current = _settings.value
        if (current.cloudToken.isBlank()) {
            if (manual) showMessage("请先登录云同步账号")
            return
        }
        if (_cloudBusy.value) return
        _cloudBusy.value = true
        viewModelScope.launch(Dispatchers.IO) {
            try {
                val meta = CloudApi.meta(current.cloudServer, current.cloudToken)
                val local = repository.all()
                val diff = cloudDiff(local, meta)
                val lastIds = prefs.getStringSet("cloud_last_ids", emptySet()) ?: emptySet()
                val deleted = lastIds - local.mapTo(mutableSetOf()) { it.id }
                withContext(Dispatchers.Main) {
                    if (startup && meta.version > 0 &&
                        meta.version == _settings.value.cloudSkipVersion
                    ) {
                        _cloudBusy.value = false
                        return@withContext
                    }
                    if (meta.version == _settings.value.cloudLastVersion &&
                        diff.localChanges == 0 && deleted.isEmpty()
                    ) {
                        _cloudBusy.value = false
                        if (manual) showMessage("云端已是最新")
                        return@withContext
                    }
                    if (meta.version <= 0 && local.isEmpty()) {
                        _cloudBusy.value = false
                        if (manual) showMessage("云端与本地都还没有数据")
                        return@withContext
                    }
                    val threshold = maxOf(4, ceil(local.size * 0.25).toInt())
                    val large = (meta.version > 0 && diff.remoteChanges >= threshold) ||
                        diff.attachmentDelta >= 5
                    if (large) {
                        _cloudBusy.value = false
                        _cloudPrompt.value = CloudPrompt(meta, diff, manual, startup, deleted)
                        return@withContext
                    }
                    performCloudSync(meta, manual, deleted)
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    _cloudBusy.value = false
                    val msg = cloudErrorMessage(e)
                    showMessage(
                        if (msg.contains("登录") || msg.contains("401")) {
                            "云同步登录已失效，请重新登录"
                        } else {
                            "云同步失败：$msg"
                        },
                    )
                }
            }
        }
    }

    fun confirmCloudSync() {
        val prompt = _cloudPrompt.value ?: return
        _cloudPrompt.value = null
        performCloudSync(prompt.meta, prompt.manual, prompt.deleted)
    }

    fun dismissCloudPrompt() {
        val prompt = _cloudPrompt.value ?: return
        if (prompt.startup) {
            prefs.edit().putInt("cloud_skip_version", prompt.meta.version).apply()
            _settings.value = _settings.value.copy(cloudSkipVersion = prompt.meta.version)
        }
        _cloudPrompt.value = null
        _cloudBusy.value = false
    }

    private fun performCloudSync(startMeta: CloudMeta, manual: Boolean, deleted: Set<String>) {
        val current = _settings.value
        if (current.cloudToken.isBlank()) {
            _cloudBusy.value = false
            return
        }
        _cloudBusy.value = true
        viewModelScope.launch(Dispatchers.IO) {
            try {
                var base = startMeta
                var report: CloudMergeReport? = null
                var result: CloudMeta? = null
                var removedByCloud = 0
                for (attempt in 0..2) {
                    if (base.version > 0) {
                        val data = CloudApi.download(current.cloudServer, current.cloudToken)
                        report = repository.mergeCloudBundle(data)
                        val localDeletions = deleted.associateWith { MojiTime.nowSeconds() }
                        removedByCloud += repository.applyCloudTombstones(
                            base.tombstones + localDeletions,
                        )
                        withContext(Dispatchers.Main) { publish() }
                    }
                    val out = repository.exportBundle()
                    try {
                        result = CloudApi.upload(
                            current.cloudServer,
                            current.cloudToken,
                            out,
                            base.version,
                            deleted.toList(),
                        )
                        break
                    } catch (e: CloudException) {
                        if (e.code == 409 && attempt < 2) {
                            base = CloudApi.meta(current.cloudServer, current.cloudToken)
                            continue
                        }
                        throw e
                    }
                }
                val uploaded = result ?: throw CloudException(409, "云端持续被其他设备更新，请稍后再试")
                val now = MojiTime.nowSeconds()
                withContext(Dispatchers.Main) {
                    prefs.edit()
                        .putInt("cloud_last_version", uploaded.version)
                        .putLong("cloud_last_sync", now)
                        .putInt("cloud_skip_version", 0)
                        .putStringSet(
                            "cloud_last_ids",
                            repository.all().mapTo(mutableSetOf()) { it.id },
                        )
                        .apply()
                    _settings.value = _settings.value.copy(
                        cloudLastVersion = uploaded.version,
                        cloudLastSync = now,
                        cloudSkipVersion = 0,
                    )
                    _cloudBusy.value = false
                    publish()
                    val merged = report
                    if (merged == null) {
                        showMessage("云同步完成 · 云端 v${uploaded.version}")
                    } else {
                        showMessage(
                            "云同步完成：新增 ${merged.added} 条，更新 ${merged.updated} 条" +
                                (if (removedByCloud > 0) "，删除 ${removedByCloud} 条" else "") +
                                "，云端 v${uploaded.version}",
                        )
                    }
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    _cloudBusy.value = false
                    val msg = cloudErrorMessage(e)
                    showMessage(
                        if (msg.contains("登录") || msg.contains("401")) {
                            "云同步登录已失效，请重新登录"
                        } else {
                            "云同步失败：$msg"
                        },
                    )
                }
            }
        }
    }

    // ------------------------------------------------------------------ 设置

    private fun loadSettings(): UiSettings {
        val config = runCatching { repository.loadConfig() }.getOrDefault(JSONObject())
        val savedTheme = prefs.getString("theme", null)
        val theme = ThemeMode.from(
            savedTheme ?: config.optString("theme", "system").takeIf { it == "light" || it == "dark" },
        )
        val accent = prefs.getString("accent", null)
            ?: config.optString("accent", "#4f6ef7")
        val dynamic = prefs.getBoolean("dynamic_color", false)
        val syncUri = prefs.getString("sync_uri", null)
        val label = syncUri?.let { syncManager.folderName(Uri.parse(it)) } ?: "未连接"
        return UiSettings(
            themeMode = theme,
            accentHex = accent,
            dynamicColor = dynamic,
            syncUri = syncUri,
            syncLabel = label,
            appName = config.optString("app_name", "墨记"),
            appSubtitle = config.optString("app_subtitle", "电子记事本"),
            brandLogo = config.optString("brand_logo", ""),
            brandTransform = config.optString("brand_logo_transform", ""),
            defaultRingtone = config.optString("ringtone", ""),
            volume = config.optInt("volume", 100).coerceIn(0, 100),
            cloudServer = prefs.getString("cloud_server", DEFAULT_CLOUD_SERVER) ?: DEFAULT_CLOUD_SERVER,
            cloudEmail = prefs.getString("cloud_email", "") ?: "",
            cloudToken = prefs.getString("cloud_token", "") ?: "",
            cloudLastVersion = prefs.getInt("cloud_last_version", 0),
            cloudLastSync = prefs.getLong("cloud_last_sync", 0L),
            cloudSkipVersion = prefs.getInt("cloud_skip_version", 0),
        )
    }

    fun setBrand(name: String, subtitle: String) {
        val cleanName = name.trim()
        val cleanSubtitle = subtitle.trim()
        _settings.value = _settings.value.copy(appName = cleanName, appSubtitle = cleanSubtitle)
        runCatching {
            repository.saveConfig(
                JSONObject().put("app_name", cleanName).put("app_subtitle", cleanSubtitle),
            )
        }
    }

    fun setBrandLogo(fileName: String, bytes: ByteArray) {
        runCatching {
            val path = repository.saveBrandLogoBytes(fileName, bytes)
            repository.saveConfig(JSONObject().put("brand_logo", path))
            val current = _settings.value
            _settings.value = current.copy(
                brandLogo = path,
                brandRevision = current.brandRevision + 1,
            )
            showMessage("Logo 已更新")
        }.onFailure { showMessage("Logo 保存失败：${it.message}") }
    }

    fun clearBrandLogo() {
        runCatching {
            repository.clearBrandLogo()
            repository.saveConfig(JSONObject().put("brand_logo", ""))
            val current = _settings.value
            _settings.value = current.copy(
                brandLogo = "",
                brandRevision = current.brandRevision + 1,
            )
        }
    }

    fun setBrandTransform(json: String) {
        _settings.value = _settings.value.copy(brandTransform = json)
        runCatching { repository.saveConfig(JSONObject().put("brand_logo_transform", json)) }
    }

    fun brandLogoFile(): java.io.File? {
        val relative = _settings.value.brandLogo
        if (relative.isBlank()) return null
        return runCatching { repository.resolve(relative) }.getOrNull()?.takeIf { it.exists() }
    }

    fun setDefaultRingtone(fileName: String, bytes: ByteArray) {
        runCatching {
            val path = repository.saveRingtoneBytes(fileName, bytes)
            repository.saveConfig(JSONObject().put("ringtone", path))
            _settings.value = _settings.value.copy(defaultRingtone = path)
            showMessage("默认铃声已更新")
        }.onFailure { showMessage("铃声保存失败：${it.message}") }
    }

    fun clearDefaultRingtone() {
        runCatching {
            repository.saveConfig(JSONObject().put("ringtone", ""))
            _settings.value = _settings.value.copy(defaultRingtone = "")
        }
    }

    fun setVolume(percent: Int) {
        val v = percent.coerceIn(0, 100)
        _settings.value = _settings.value.copy(volume = v)
        runCatching { repository.saveConfig(JSONObject().put("volume", v)) }
    }

    fun setEventRingtone(eventId: String, fileName: String, bytes: ByteArray) {
        mutate {
            val path = repository.saveRingtoneBytes(fileName, bytes)
            repository.setRingtone(eventId, path)
        }
        showMessage("该事项的铃声已更新")
    }

    fun clearEventRingtone(eventId: String) {
        mutate { repository.setRingtone(eventId, "") }
    }

    /** 编辑器取消时，删除已经复制但尚未绑定到事项的铃声。 */
    fun deletePendingRingtone(relativePath: String?) {
        if (relativePath.isNullOrBlank()) return
        runCatching { repository.deleteRingtone(relativePath) }
    }

    fun setEventRingtonePath(eventId: String, relativePath: String) {
        mutate { repository.setRingtone(eventId, relativePath) }
    }

    fun snooze(id: String, minutes: Int = 5) {
        val until = MojiTime.nowSeconds() + minutes.coerceAtLeast(1) * 60L
        mutate { repository.setSnooze(id, until) }
        showMessage("$minutes 分钟后再提醒")
    }

    fun clearSnooze(id: String) {
        mutate { repository.setSnooze(id, null) }
    }

    fun ringtoneFile(relative: String): java.io.File? {
        if (relative.isBlank()) return null
        return runCatching { repository.resolve(relative) }.getOrNull()?.takeIf { it.exists() }
    }

    fun setThemeMode(mode: ThemeMode) {
        prefs.edit().putString("theme", mode.stored).apply()
        _settings.value = _settings.value.copy(themeMode = mode)
        val systemDark = (getApplication<Application>().resources.configuration.uiMode and
            android.content.res.Configuration.UI_MODE_NIGHT_MASK) ==
            android.content.res.Configuration.UI_MODE_NIGHT_YES
        runCatching {
            repository.saveConfig(
                JSONObject().put(
                    "theme",
                    if (mode == ThemeMode.SYSTEM) {
                        if (systemDark) "dark" else "light"
                    } else {
                        mode.stored
                    },
                ),
            )
        }
    }

    fun setAccent(hex: String) {
        prefs.edit().putString("accent", hex).apply()
        _settings.value = _settings.value.copy(accentHex = hex)
        runCatching { repository.saveConfig(JSONObject().put("accent", hex)) }
    }

    fun setDynamicColor(enabled: Boolean) {
        prefs.edit().putBoolean("dynamic_color", enabled).apply()
        _settings.value = _settings.value.copy(dynamicColor = enabled)
    }

    fun dataPath(): String = repository.dataPath()

    fun backupCount(): Int = repository.backupFiles().size

    fun showMessagePublic(text: String) = showMessage(text)
}

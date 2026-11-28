package com.moji.note.ui.screens

import android.app.DatePickerDialog
import android.app.TimePickerDialog
import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.MusicNote
import androidx.compose.material.icons.filled.Remove
import androidx.compose.material.icons.outlined.AttachFile
import androidx.compose.material.icons.outlined.CalendarMonth
import androidx.compose.material.icons.outlined.DeleteOutline
import androidx.compose.material.icons.outlined.Save
import androidx.compose.material3.AssistChip
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.moji.note.data.Attachment
import com.moji.note.data.MojiTime
import com.moji.note.data.guessMime
import com.moji.note.data.splitTags
import com.moji.note.ui.DraftAttachment
import com.moji.note.ui.MojiViewModel
import com.moji.note.ui.components.AttachmentThumbnail
import com.moji.note.ui.components.ConfirmDialog
import com.moji.note.ui.components.MarkdownView
import com.moji.note.ui.components.humanSize
import com.moji.note.ui.theme.AccentOptions
import com.moji.note.ui.theme.parseColor
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.util.Calendar

@Composable
fun EditorScreen(
    vm: MojiViewModel,
    eventId: String?,
    onClose: () -> Unit,
    onSaved: (String) -> Unit,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val initial = remember(eventId) { eventId?.let(vm::find) }
    val liveEvents by vm.events.collectAsStateWithLifecycle()
    val liveEvent = eventId?.let { id -> liveEvents.firstOrNull { it.id == id } }
    val tagStats by vm.tags.collectAsStateWithLifecycle()
    val existingAttachments = liveEvent?.attachments ?: emptyList()

    if (eventId != null && initial == null) {
        Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text("记录不存在或已被删除", style = MaterialTheme.typography.titleMedium)
                Spacer(Modifier.height(12.dp))
                OutlinedButton(onClick = onClose) { Text("返回") }
            }
        }
        return
    }

    var title by rememberSaveable(eventId) { mutableStateOf(initial?.title ?: "") }
    var note by rememberSaveable(eventId) { mutableStateOf(initial?.note ?: "") }
    var due by rememberSaveable(eventId) { mutableLongStateOf(initial?.due ?: -1L) }
    var priority by rememberSaveable(eventId) { mutableIntStateOf(initial?.priority ?: 1) }
    var position by rememberSaveable(eventId) { mutableIntStateOf(vm.repository.all().size + 1) }
    var remindText by rememberSaveable(eventId) {
        mutableStateOf((initial?.remindBefore ?: 0).toString())
    }
    var tagsText by rememberSaveable(eventId) {
        mutableStateOf(initial?.tags?.joinToString(", ") ?: "")
    }
    var color by rememberSaveable(eventId) { mutableStateOf(initial?.color ?: "") }
    var icon by rememberSaveable(eventId) { mutableStateOf(initial?.icon ?: "") }
    var systemNotify by rememberSaveable(eventId) {
        mutableStateOf(initial?.systemNotify ?: true)
    }
    // 只保存相对路径，旋转屏幕后仍能找回已复制到 ringtones/ 的音频文件。
    var pendingRingtonePath by rememberSaveable(eventId) { mutableStateOf<String?>(null) }
    var pendingRingtoneName by rememberSaveable(eventId) { mutableStateOf<String?>(null) }
    var clearRingtone by rememberSaveable(eventId) { mutableStateOf(false) }
    val pageLayout = initial?.pageLayout ?: ""
    var tab by rememberSaveable(eventId) { mutableIntStateOf(0) }
    var dirty by rememberSaveable(eventId) { mutableStateOf(false) }
    var confirmDiscard by remember { mutableStateOf(false) }
    val pending = vm.draftAttachments
    val maxPosition = vm.repository.all().size + 1

    val picker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenMultipleDocuments(),
    ) { uris ->
        if (uris.isEmpty()) return@rememberLauncherForActivityResult
        scope.launch {
            val read = withContext(Dispatchers.IO) {
                uris.mapNotNull { uri -> readUri(context, uri) }
            }
            if (eventId == null) {
                pending.addAll(read)
                dirty = true
            } else {
                read.forEach {
                    vm.addAttachmentBytes(eventId, it.name, it.bytes, it.mime)
                }
            }
        }
    }

    val ringtonePicker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument(),
    ) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        scope.launch {
            val read = withContext(Dispatchers.IO) { readUri(context, uri) }
            if (read == null) {
                vm.showMessagePublic("无法读取这个音频")
                return@launch
            }
            val path = withContext(Dispatchers.IO) {
                runCatching { vm.repository.saveRingtoneBytes(read.name, read.bytes) }.getOrNull()
            }
            if (path == null) {
                vm.showMessagePublic("铃声保存失败")
                return@launch
            }
            // 连续选择时，删掉上一份尚未保存的副本。
            val previous = pendingRingtonePath
            if (previous != null && previous != path) vm.deletePendingRingtone(previous)
            pendingRingtonePath = path
            pendingRingtoneName = read.name
            clearRingtone = false
            dirty = true
        }
    }

    fun save() {
        val clean = title.trim()
        if (clean.isEmpty()) {
            vm.showMessagePublic("标题不能为空")
            return
        }
        val minutes = remindText.toIntOrNull()?.coerceAtLeast(0) ?: 0
        val newId = vm.saveEvent(
            id = eventId,
            title = clean,
            note = note,
            due = if (due > 0) due else null,
            tags = splitTags(tagsText),
            remindBefore = minutes,
            color = color,
            icon = icon,
            systemNotify = systemNotify,
            position = if (eventId == null) position else null,
            pageLayout = pageLayout,
        )
        if (newId != null) {
            pending.forEach { vm.addAttachmentBytes(newId, it.name, it.bytes, it.mime) }
            val ringtonePath = pendingRingtonePath
            if (ringtonePath != null) {
                vm.setEventRingtonePath(newId, ringtonePath)
            } else if (clearRingtone) {
                vm.clearEventRingtone(newId)
            }
            vm.clearDraftAttachments()
            onSaved(newId)
        }
    }

    fun discardAndClose() {
        vm.deletePendingRingtone(pendingRingtonePath)
        pendingRingtonePath = null
        vm.clearDraftAttachments()
        onClose()
    }

    BackHandler {
        if (dirty) confirmDiscard = true else discardAndClose()
    }

    Column(Modifier.fillMaxSize()) {
        Surface(color = MaterialTheme.colorScheme.background, tonalElevation = 0.dp) {
            Row(
                Modifier
                    .fillMaxWidth()
                    .statusBarsPadding()
                    .padding(horizontal = 6.dp, vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                IconButton(onClick = { if (dirty) confirmDiscard = true else discardAndClose() }) {
                    Icon(Icons.Filled.ArrowBack, "返回")
                }
                Column(Modifier.weight(1f)) {
                    Text(
                        if (eventId == null) "新建待办" else "编辑待办",
                        style = MaterialTheme.typography.titleLarge,
                    )
                    if (dirty) {
                        Text(
                            "尚未保存",
                            style = MaterialTheme.typography.labelMedium,
                            color = MaterialTheme.colorScheme.primary,
                        )
                    }
                }
                TextButton(onClick = { save() }) {
                    Icon(Icons.Outlined.Save, null, modifier = Modifier.size(18.dp))
                    Spacer(Modifier.width(5.dp))
                    Text("保存", fontWeight = FontWeight.SemiBold)
                }
            }
        }
        Column(
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .imePadding()
                .padding(horizontal = 16.dp)
                .padding(bottom = 32.dp),
        ) {
            OutlinedTextField(
                value = title,
                onValueChange = { title = it; dirty = true },
                label = { Text("标题") },
                placeholder = { Text("要做什么？") },
                singleLine = true,
                shape = RoundedCornerShape(16.dp),
                modifier = Modifier.fillMaxWidth(),
            )
            Spacer(Modifier.height(14.dp))
            SectionCard {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    FilterChip(
                        selected = tab == 0,
                        onClick = { tab = 0 },
                        label = { Text("Markdown") },
                        shape = RoundedCornerShape(10.dp),
                    )
                    Spacer(Modifier.width(8.dp))
                    FilterChip(
                        selected = tab == 1,
                        onClick = { tab = 1 },
                        label = { Text("预览") },
                        shape = RoundedCornerShape(10.dp),
                    )
                    Spacer(Modifier.weight(1f))
                    Text(
                        "${note.length} 字符",
                        style = MaterialTheme.typography.labelMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                Spacer(Modifier.height(10.dp))
                if (tab == 0) {
                    OutlinedTextField(
                        value = note,
                        onValueChange = { note = it; dirty = true },
                        placeholder = { Text("支持 Markdown：标题、列表、表格、代码块…") },
                        textStyle = MaterialTheme.typography.bodyMedium.copy(
                            fontFamily = FontFamily.Monospace,
                            lineHeight = 21.sp,
                        ),
                        minLines = 8,
                        maxLines = 20,
                        shape = RoundedCornerShape(14.dp),
                        modifier = Modifier.fillMaxWidth(),
                    )
                } else {
                    Surface(
                        shape = RoundedCornerShape(14.dp),
                        color = MaterialTheme.colorScheme.surfaceContainerLow,
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        MarkdownView(
                            markdown = note,
                            modifier = Modifier.padding(12.dp),
                        )
                    }
                }
            }
            Spacer(Modifier.height(14.dp))
            SectionCard {
                FieldLabel("截止时间")
                Surface(
                    onClick = {
                        showDateTimePicker(context, if (due > 0) due else null) {
                            due = it
                            dirty = true
                        }
                    },
                    shape = RoundedCornerShape(14.dp),
                    color = MaterialTheme.colorScheme.surfaceContainerLow,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Row(
                        Modifier.padding(horizontal = 14.dp, vertical = 13.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Icon(
                            Icons.Outlined.CalendarMonth,
                            null,
                            tint = MaterialTheme.colorScheme.primary,
                            modifier = Modifier.size(19.dp),
                        )
                        Spacer(Modifier.width(10.dp))
                        Text(
                            if (due > 0) MojiTime.fullDateTime(due) else "设置日期与时间",
                            style = MaterialTheme.typography.bodyMedium,
                            color = if (due > 0) MaterialTheme.colorScheme.onSurface
                            else MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.weight(1f),
                        )
                        if (due > 0) {
                            IconButton(
                                onClick = { due = -1L; dirty = true },
                                modifier = Modifier.size(28.dp),
                            ) {
                                Icon(Icons.Filled.Close, "清除截止时间", modifier = Modifier.size(17.dp))
                            }
                        }
                    }
                }
                Spacer(Modifier.height(8.dp))
                LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    item {
                        QuickTimeChip("今天 18:00") { setQuickDue(0, 18) { due = it; dirty = true } }
                    }
                    item {
                        QuickTimeChip("明天 09:00") { setQuickDue(1, 9) { due = it; dirty = true } }
                    }
                    item {
                        QuickTimeChip("明天 18:00") { setQuickDue(1, 18) { due = it; dirty = true } }
                    }
                    item {
                        QuickTimeChip("后天 09:00") { setQuickDue(2, 9) { due = it; dirty = true } }
                    }
                    item {
                        QuickTimeChip("一周后") { setQuickDue(7, 9) { due = it; dirty = true } }
                    }
                }
                Spacer(Modifier.height(16.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        FieldLabel("优先级")
                        Stepper(
                            value = priority,
                            min = 1,
                            max = 99,
                            onChange = { priority = it; dirty = true },
                        )
                    }
                    if (eventId == null) {
                        Column(Modifier.weight(1f)) {
                            FieldLabel("插入位置")
                            Stepper(
                                value = position,
                                min = 1,
                                max = maxPosition,
                                onChange = { position = it; dirty = true },
                            )
                        }
                    }
                }
                Spacer(Modifier.height(16.dp))
                FieldLabel("提前提醒")
                Row(verticalAlignment = Alignment.CenterVertically) {
                    OutlinedTextField(
                        value = remindText,
                        onValueChange = {
                            if (it.all { c -> c.isDigit() } && it.length <= 5) {
                                remindText = it
                                dirty = true
                            }
                        },
                        singleLine = true,
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        suffix = { Text("分钟") },
                        shape = RoundedCornerShape(12.dp),
                        modifier = Modifier.width(150.dp),
                    )
                    Spacer(Modifier.width(12.dp))
                    Text(
                        if ((remindText.toIntOrNull() ?: 0) == 0) "到期时提醒"
                        else "提前 ${remindText.toIntOrNull() ?: 0} 分钟提醒",
                        style = MaterialTheme.typography.labelMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                Spacer(Modifier.height(8.dp))
                LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    items(listOf(0, 5, 15, 30, 60, 1440)) { minutes ->
                        FilterChip(
                            selected = (remindText.toIntOrNull() ?: 0) == minutes,
                            onClick = { remindText = minutes.toString(); dirty = true },
                            label = {
                                Text(if (minutes == 0) "准时" else if (minutes >= 60) "${minutes / 60} 小时" else "$minutes 分钟")
                            },
                            shape = RoundedCornerShape(10.dp),
                        )
                    }
                }
                Spacer(Modifier.height(16.dp))
                FieldLabel("该事项铃声")
                Surface(
                    shape = RoundedCornerShape(14.dp),
                    color = MaterialTheme.colorScheme.surfaceContainerLow,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Row(
                        Modifier.padding(horizontal = 14.dp, vertical = 12.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Icon(
                            Icons.Filled.MusicNote,
                            null,
                            tint = MaterialTheme.colorScheme.primary,
                            modifier = Modifier.size(19.dp),
                        )
                        Spacer(Modifier.width(10.dp))
                        val currentRingtone = pendingRingtoneName
                            ?: if (clearRingtone) null
                            else liveEvent?.ringtone?.substringAfterLast('/')?.takeIf { it.isNotBlank() }
                        Text(
                            currentRingtone ?: "跟随默认铃声",
                            style = MaterialTheme.typography.bodyMedium,
                            color = if (currentRingtone == null) MaterialTheme.colorScheme.onSurfaceVariant
                            else MaterialTheme.colorScheme.onSurface,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                            modifier = Modifier.weight(1f),
                        )
                        TextButton(onClick = { ringtonePicker.launch(arrayOf("audio/*")) }) {
                            Text(if (currentRingtone == null) "选择" else "更换")
                        }
                        if (currentRingtone != null) {
                            TextButton(onClick = {
                                vm.deletePendingRingtone(pendingRingtonePath)
                                pendingRingtonePath = null
                                pendingRingtoneName = null
                                clearRingtone = true
                                dirty = true
                            }) { Text("清除") }
                        }
                    }
                }
                Spacer(Modifier.height(16.dp))
                FieldLabel("标签")
                OutlinedTextField(
                    value = tagsText,
                    onValueChange = { tagsText = it; dirty = true },
                    placeholder = { Text("用逗号或空格分隔，例如：课程, 截止") },
                    singleLine = true,
                    shape = RoundedCornerShape(14.dp),
                    modifier = Modifier.fillMaxWidth(),
                )
                if (tagStats.isNotEmpty()) {
                    Spacer(Modifier.height(8.dp))
                    LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        items(tagStats.entries.sortedByDescending { it.value }.take(12).toList()) { entry ->
                            val current = splitTags(tagsText)
                            AssistChip(
                                onClick = {
                                    tagsText = if (entry.key in current) {
                                        current.filter { it != entry.key }.joinToString(", ")
                                    } else {
                                        (current + entry.key).joinToString(", ")
                                    }
                                    dirty = true
                                },
                                label = { Text(if (entry.key in current) "✓ #${entry.key}" else "#${entry.key}") },
                            )
                        }
                    }
                }
                Spacer(Modifier.height(16.dp))
                FieldLabel("色条")
                LazyRow(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    item {
                        ColorSwatch("", selected = color.isEmpty()) { color = ""; dirty = true }
                    }
                    items(AccentOptions) { hex ->
                        ColorSwatch(hex, selected = color == hex) { color = hex; dirty = true }
                    }
                }
                Spacer(Modifier.height(16.dp))
                FieldLabel("图标")
                Row(verticalAlignment = Alignment.CenterVertically) {
                    OutlinedTextField(
                        value = icon,
                        onValueChange = { if (it.length <= 4) { icon = it; dirty = true } },
                        singleLine = true,
                        placeholder = { Text("emoji") },
                        shape = RoundedCornerShape(12.dp),
                        modifier = Modifier.width(130.dp),
                    )
                    Spacer(Modifier.width(10.dp))
                    LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        items(listOf("📌", "📚", "💡", "✅", "🛒", "🎯", "🎵", "✈️", "💰", "🏃")) { emoji ->
                            Surface(
                                onClick = { icon = emoji; dirty = true },
                                shape = CircleShape,
                                color = if (icon == emoji) MaterialTheme.colorScheme.primaryContainer
                                else MaterialTheme.colorScheme.surfaceContainerLow,
                                modifier = Modifier.size(40.dp),
                            ) {
                                Box(contentAlignment = Alignment.Center) {
                                    Text(emoji, fontSize = 19.sp)
                                }
                            }
                        }
                    }
                }
                Spacer(Modifier.height(14.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text("PC 端系统通知", style = MaterialTheme.typography.titleMedium)
                        Text(
                            "同步到 PC 后是否在 Windows 通知中心弹出；Android 端到期仍会响闹钟",
                            style = MaterialTheme.typography.labelMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    Switch(
                        checked = systemNotify,
                        onCheckedChange = { systemNotify = it; dirty = true },
                    )
                }
            }
            Spacer(Modifier.height(14.dp))
            SectionCard {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    FieldLabel("附件", Modifier.weight(1f))
                    IconButton(onClick = { picker.launch(arrayOf("*/*")) }) {
                        Icon(
                            Icons.Outlined.AttachFile,
                            "添加附件",
                            tint = MaterialTheme.colorScheme.primary,
                        )
                    }
                }
                if (existingAttachments.isEmpty() && pending.isEmpty()) {
                    Text(
                        "支持图片、音频、PDF 等文件，保存在 attachments/<id>/ 目录。",
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                existingAttachments.forEachIndexed { index, attachment ->
                    AttachmentRow(
                        attachment = attachment,
                        file = vm.repository.resolveAttachment(attachment),
                        onRemove = { vm.removeAttachment(eventId!!, index) },
                    )
                }
                pending.forEachIndexed { index, item ->
                    AttachmentRow(
                        attachment = Attachment(
                            path = "",
                            name = item.name,
                            mime = item.mime,
                            size = item.bytes.size.toLong(),
                        ),
                        file = null,
                        onRemove = { pending.removeAt(index) },
                    )
                }
            }
            Spacer(Modifier.height(20.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                OutlinedButton(
                    onClick = { if (dirty) confirmDiscard = true else discardAndClose() },
                    modifier = Modifier.weight(1f),
                ) { Text("取消") }
                Button(onClick = { save() }, modifier = Modifier.weight(2f)) {
                    Icon(Icons.Outlined.Save, null, modifier = Modifier.size(18.dp))
                    Spacer(Modifier.width(6.dp))
                    Text(if (eventId == null) "创建待办" else "保存修改")
                }
            }
        }
    }

    if (confirmDiscard) {
        ConfirmDialog(
            title = "放弃未保存的修改？",
            text = "当前编辑内容尚未保存，返回后将会丢失。",
            confirmLabel = "放弃",
            onConfirm = { discardAndClose() },
            onDismiss = { confirmDiscard = false },
        )
    }
}

@Composable
private fun SectionCard(content: @Composable () -> Unit) {
    Surface(
        shape = RoundedCornerShape(22.dp),
        color = MaterialTheme.colorScheme.surface,
        shadowElevation = 1.dp,
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(Modifier.padding(16.dp)) { content() }
    }
}

@Composable
private fun FieldLabel(text: String, modifier: Modifier = Modifier) {
    Text(
        text,
        style = MaterialTheme.typography.labelLarge,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        fontWeight = FontWeight.SemiBold,
        modifier = modifier.padding(bottom = 7.dp),
    )
}

@Composable
private fun Stepper(value: Int, min: Int, max: Int, onChange: (Int) -> Unit) {
    Surface(
        shape = RoundedCornerShape(12.dp),
        color = MaterialTheme.colorScheme.surfaceContainerLow,
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            IconButton(
                onClick = { onChange((value - 1).coerceIn(min, max)) },
                enabled = value > min,
                modifier = Modifier.size(38.dp),
            ) {
                Icon(Icons.Filled.Remove, "减少", modifier = Modifier.size(18.dp))
            }
            Text(
                value.toString(),
                style = MaterialTheme.typography.titleMedium,
                modifier = Modifier.width(34.dp),
                textAlign = androidx.compose.ui.text.style.TextAlign.Center,
            )
            IconButton(
                onClick = { onChange((value + 1).coerceIn(min, max)) },
                enabled = value < max,
                modifier = Modifier.size(38.dp),
            ) {
                Icon(Icons.Filled.Add, "增加", modifier = Modifier.size(18.dp))
            }
        }
    }
}

@Composable
private fun QuickTimeChip(label: String, onClick: () -> Unit) {
    AssistChip(onClick = onClick, label = { Text(label) })
}

@Composable
private fun ColorSwatch(hex: String, selected: Boolean, onClick: () -> Unit) {
    val color = if (hex.isEmpty()) MaterialTheme.colorScheme.primary else parseColor(hex)
    Box(
        Modifier
            .size(34.dp)
            .clip(CircleShape)
            .background(color)
            .then(
                if (selected) Modifier.border(3.dp, MaterialTheme.colorScheme.onSurface, CircleShape)
                else Modifier.border(1.dp, MaterialTheme.colorScheme.outlineVariant, CircleShape),
            )
            .clickable { onClick() },
        contentAlignment = Alignment.Center,
    ) {
        if (hex.isEmpty()) {
            Text("A", color = MaterialTheme.colorScheme.onPrimary, fontSize = 14.sp)
        }
    }
}

@Composable
private fun AttachmentRow(attachment: Attachment, file: File?, onRemove: () -> Unit) {
    Row(
        Modifier
            .fillMaxWidth()
            .padding(vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        AttachmentThumbnail(attachment, file, size = 46)
        Spacer(Modifier.width(10.dp))
        Column(Modifier.weight(1f)) {
            Text(
                attachment.name,
                style = MaterialTheme.typography.bodyMedium,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                "${attachment.mime.ifBlank { "文件" }} · ${humanSize(attachment.size)}",
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        IconButton(onClick = onRemove) {
            Icon(
                Icons.Outlined.DeleteOutline,
                "移除附件",
                tint = MaterialTheme.colorScheme.error,
                modifier = Modifier.size(20.dp),
            )
        }
    }
}

private fun showDateTimePicker(context: Context, initial: Long?, onPicked: (Long) -> Unit) {
    val calendar = Calendar.getInstance()
    if (initial != null) calendar.timeInMillis = initial * 1000L
    DatePickerDialog(
        context,
        { _, year, month, day ->
            TimePickerDialog(
                context,
                { _, hour, minute ->
                    val picked = Calendar.getInstance().apply {
                        set(year, month, day, hour, minute, 0)
                        set(Calendar.MILLISECOND, 0)
                    }
                    onPicked(picked.timeInMillis / 1000L)
                },
                calendar.get(Calendar.HOUR_OF_DAY),
                calendar.get(Calendar.MINUTE),
                true,
            ).show()
        },
        calendar.get(Calendar.YEAR),
        calendar.get(Calendar.MONTH),
        calendar.get(Calendar.DAY_OF_MONTH),
    ).show()
}

private fun setQuickDue(daysFromNow: Int, hour: Int, onPicked: (Long) -> Unit) {
    val calendar = Calendar.getInstance().apply {
        add(Calendar.DAY_OF_YEAR, daysFromNow)
        set(Calendar.HOUR_OF_DAY, hour)
        set(Calendar.MINUTE, 0)
        set(Calendar.SECOND, 0)
        set(Calendar.MILLISECOND, 0)
    }
    onPicked(calendar.timeInMillis / 1000L)
}

private fun readUri(context: Context, uri: Uri): DraftAttachment? = runCatching {
    val resolver = context.contentResolver
    var name = "file"
    resolver.query(uri, null, null, null, null)?.use { cursor ->
        if (cursor.moveToFirst()) {
            val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            if (index >= 0) name = cursor.getString(index) ?: name
        }
    }
    val bytes = resolver.openInputStream(uri)?.use { it.readBytes() }
        ?: throw IllegalStateException("无法读取附件")
    val mime = resolver.getType(uri) ?: guessMime(name)
    DraftAttachment(name, mime, bytes)
}.getOrNull()

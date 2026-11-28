package com.moji.note.ui.screens

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.DeleteOutline
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.RadioButtonUnchecked
import androidx.compose.material.icons.outlined.AttachFile
import androidx.compose.material.icons.outlined.CalendarMonth
import androidx.compose.material.icons.outlined.Close
import androidx.compose.material.icons.outlined.Flag
import androidx.compose.material.icons.outlined.Label
import androidx.compose.material.icons.outlined.MusicNote
import androidx.compose.material.icons.outlined.NotificationsActive
import androidx.compose.material.icons.outlined.OpenInNew
import androidx.compose.material.icons.outlined.Schedule
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRow
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.moji.note.data.Attachment
import com.moji.note.data.MojiTime
import com.moji.note.data.guessMime
import com.moji.note.ui.MojiViewModel
import com.moji.note.ui.components.AttachmentThumbnail
import com.moji.note.ui.components.ConfirmDialog
import com.moji.note.ui.components.MarkdownView
import com.moji.note.ui.components.decodeImage
import com.moji.note.ui.components.humanSize
import com.moji.note.ui.components.openAttachment
import com.moji.note.ui.theme.parseColor
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

@Composable
fun DetailScreen(
    vm: MojiViewModel,
    eventId: String,
    onBack: () -> Unit,
    onEdit: (String) -> Unit,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val events by vm.events.collectAsStateWithLifecycle()
    val event = events.firstOrNull { it.id == eventId }
    var tab by rememberSaveable(eventId) { mutableIntStateOf(0) }
    var confirmDelete by remember { mutableStateOf(false) }
    var preview by remember { mutableStateOf<Attachment?>(null) }
    var noteSource by rememberSaveable(eventId) { mutableStateOf(false) }

    if (event == null) {
        Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text("记录不存在或已被删除", style = MaterialTheme.typography.titleMedium)
                Spacer(Modifier.height(12.dp))
                OutlinedButton(onClick = onBack) { Text("返回") }
            }
        }
        return
    }

    val picker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenMultipleDocuments(),
    ) { uris ->
        if (uris.isEmpty()) return@rememberLauncherForActivityResult
        scope.launch {
            val read = withContext(Dispatchers.IO) {
                uris.mapNotNull { uri -> readUri(context, uri) }
            }
            read.forEach { triple ->
                vm.addAttachmentBytes(event.id, triple.first, triple.second, triple.third)
            }
        }
    }

    BackHandler { onBack() }

    Column(Modifier.fillMaxSize()) {
        Row(
            Modifier
                .fillMaxWidth()
                .statusBarsPadding()
                .padding(horizontal = 6.dp, vertical = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            IconButton(onClick = onBack) { Icon(Icons.Filled.ArrowBack, "返回") }
            Text(
                "事项详情",
                style = MaterialTheme.typography.titleLarge,
                modifier = Modifier.weight(1f),
            )
            IconButton(onClick = { onEdit(event.id) }) {
                Icon(Icons.Filled.Edit, "编辑", tint = MaterialTheme.colorScheme.primary)
            }
            IconButton(onClick = { confirmDelete = true }) {
                Icon(Icons.Filled.DeleteOutline, "删除", tint = MaterialTheme.colorScheme.error)
            }
        }
        TabRow(
            selectedTabIndex = tab,
            containerColor = MaterialTheme.colorScheme.background,
        ) {
            Tab(selected = tab == 0, onClick = { tab = 0 }, text = { Text("详情") })
            Tab(selected = tab == 1, onClick = { tab = 1 }, text = { Text("页面与墨迹") })
        }
        if (tab == 0) {
            Column(
                Modifier
                    .fillMaxSize()
                    .verticalScroll(rememberScrollState())
                    .padding(horizontal = 16.dp)
                    .padding(bottom = 32.dp),
            ) {
                Spacer(Modifier.height(12.dp))
                Surface(
                    shape = RoundedCornerShape(22.dp),
                    color = MaterialTheme.colorScheme.surface,
                    shadowElevation = 1.dp,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Column(Modifier.padding(18.dp)) {
                        Row(verticalAlignment = Alignment.Top) {
                            Box(
                                Modifier
                                    .size(4.dp)
                                    .height(50.dp)
                                    .clip(RoundedCornerShape(2.dp))
                                    .background(
                                        if (event.color.isNotBlank()) parseColor(event.color)
                                        else MaterialTheme.colorScheme.primary,
                                    ),
                            )
                            Spacer(Modifier.width(12.dp))
                            Column(Modifier.weight(1f)) {
                                Text(
                                    buildString {
                                        if (event.icon.isNotBlank()) append(event.icon).append(' ')
                                        append(event.title)
                                    },
                                    style = MaterialTheme.typography.headlineSmall,
                                    fontWeight = FontWeight.Bold,
                                    color = if (event.done) MaterialTheme.colorScheme.onSurfaceVariant
                                    else MaterialTheme.colorScheme.onSurface,
                                )
                                Spacer(Modifier.height(8.dp))
                                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                    StatusPill(
                                        text = event.badge(),
                                        color = when {
                                            event.done -> MaterialTheme.colorScheme.onSurfaceVariant
                                            event.isOverdue() -> MaterialTheme.colorScheme.error
                                            event.isDueToday() -> MaterialTheme.colorScheme.tertiary
                                            else -> MaterialTheme.colorScheme.primary
                                        },
                                    )
                                    StatusPill(
                                        text = "P${event.priority}",
                                        color = MaterialTheme.colorScheme.secondary,
                                    )
                                    if (event.attachments.isNotEmpty()) {
                                        StatusPill(
                                            text = "${event.attachments.size} 个附件",
                                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                                        )
                                    }
                                }
                            }
                        }
                        Spacer(Modifier.height(16.dp))
                        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                            Button(
                                onClick = { vm.setDone(event.id, !event.done) },
                                modifier = Modifier.weight(1f),
                            ) {
                                Icon(
                                    if (event.done) Icons.Filled.RadioButtonUnchecked
                                    else Icons.Filled.CheckCircle,
                                    null,
                                    modifier = Modifier.size(18.dp),
                                )
                                Spacer(Modifier.width(6.dp))
                                Text(if (event.done) "标记为未完成" else "标记为完成")
                            }
                            OutlinedButton(
                                onClick = { onEdit(event.id) },
                                modifier = Modifier.weight(1f),
                            ) {
                                Icon(Icons.Filled.Edit, null, modifier = Modifier.size(17.dp))
                                Spacer(Modifier.width(6.dp))
                                Text("编辑")
                            }
                        }
                    }
                }
                Spacer(Modifier.height(12.dp))
                Surface(
                    shape = RoundedCornerShape(22.dp),
                    color = MaterialTheme.colorScheme.surface,
                    shadowElevation = 1.dp,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Column(Modifier.padding(16.dp)) {
                        MetaRow(
                            icon = Icons.Outlined.CalendarMonth,
                            label = "截止时间",
                            value = if (event.due == null) "无期限"
                            else "${MojiTime.fullDateTime(event.due)}（${event.dueText()}）",
                            color = if (event.isOverdue()) MaterialTheme.colorScheme.error
                            else MaterialTheme.colorScheme.onSurface,
                        )
                        val snooze = event.snoozeUntil?.takeIf { it > MojiTime.nowSeconds() }
                        MetaRow(
                            icon = Icons.Outlined.Schedule,
                            label = "提醒",
                            value = buildString {
                                append(
                                    if (event.remindBefore == 0) "到期时提醒"
                                    else "提前 ${event.remindBefore} 分钟",
                                )
                                snooze?.let { append(" · 稍后 ${MojiTime.clock(it)}") }
                            },
                        )
                        MetaRow(
                            icon = Icons.Outlined.NotificationsActive,
                            label = "PC 通知",
                            value = if (event.systemNotify) "已开启" else "未开启（Android 仍会响闹钟）",
                        )
                        MetaRow(
                            icon = Icons.Outlined.MusicNote,
                            label = "铃声",
                            value = if (event.ringtone.isBlank()) "跟随默认铃声"
                            else event.ringtone.substringAfterLast('/'),
                        )
                        MetaRow(
                            icon = Icons.Outlined.Flag,
                            label = "优先级",
                            value = "P${event.priority}（第 ${event.priority} 位）",
                        )
                        if (event.tags.isNotEmpty()) {
                            MetaRow(
                                icon = Icons.Outlined.Label,
                                label = "标签",
                                value = event.tags.joinToString("  ") { "#$it" },
                            )
                        }
                        Text(
                            "创建 ${event.createdAt?.let { MojiTime.fullDateTime(it) } ?: "-"}  ·  " +
                                "更新 ${event.updatedAt?.let { MojiTime.fullDateTime(it) } ?: "-"}  ·  " +
                                "id ${event.id}",
                            style = MaterialTheme.typography.labelMedium,
                            fontFamily = FontFamily.Monospace,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.padding(top = 8.dp),
                        )
                    }
                }
                Spacer(Modifier.height(12.dp))
                Surface(
                    shape = RoundedCornerShape(22.dp),
                    color = MaterialTheme.colorScheme.surface,
                    shadowElevation = 1.dp,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Column(Modifier.padding(16.dp)) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text(
                                "备注",
                                style = MaterialTheme.typography.titleMedium,
                                fontWeight = FontWeight.SemiBold,
                                modifier = Modifier.weight(1f),
                            )
                            if (event.note.isNotBlank()) {
                                FilterChip(
                                    selected = !noteSource,
                                    onClick = { noteSource = false },
                                    label = { Text("渲染") },
                                    shape = RoundedCornerShape(9.dp),
                                )
                                Spacer(Modifier.width(6.dp))
                                FilterChip(
                                    selected = noteSource,
                                    onClick = { noteSource = true },
                                    label = { Text("源码") },
                                    shape = RoundedCornerShape(9.dp),
                                )
                            }
                        }
                        Spacer(Modifier.height(8.dp))
                        if (event.note.isBlank()) {
                            Text(
                                "没有备注内容",
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        } else if (noteSource) {
                            Surface(
                                shape = RoundedCornerShape(12.dp),
                                color = MaterialTheme.colorScheme.surfaceContainerLow,
                                modifier = Modifier.fillMaxWidth(),
                            ) {
                                SelectionContainer {
                                    Text(
                                        event.note,
                                        fontFamily = FontFamily.Monospace,
                                        fontSize = 13.sp,
                                        lineHeight = 20.sp,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                                        modifier = Modifier.padding(12.dp),
                                    )
                                }
                            }
                        } else {
                            MarkdownView(markdown = event.note)
                        }
                    }
                }
                Spacer(Modifier.height(12.dp))
                Surface(
                    shape = RoundedCornerShape(22.dp),
                    color = MaterialTheme.colorScheme.surface,
                    shadowElevation = 1.dp,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Column(Modifier.padding(16.dp)) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text(
                                "附件 · ${event.attachments.size}",
                                style = MaterialTheme.typography.titleMedium,
                                fontWeight = FontWeight.SemiBold,
                                modifier = Modifier.weight(1f),
                            )
                            IconButton(onClick = { picker.launch(arrayOf("*/*")) }) {
                                Icon(
                                    Icons.Outlined.AttachFile,
                                    "添加附件",
                                    tint = MaterialTheme.colorScheme.primary,
                                )
                            }
                        }
                        if (event.attachments.isEmpty()) {
                            Text(
                                "暂无附件。添加的图片会自动出现在「页面与墨迹」中。",
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                        event.attachments.forEachIndexed { index, attachment ->
                            val file = vm.repository.resolveAttachment(attachment)
                            Row(
                                Modifier
                                    .fillMaxWidth()
                                    .clip(RoundedCornerShape(14.dp))
                                    .clickable {
                                        if (attachment.isImage) preview = attachment
                                        else file?.let { openAttachment(context, it, attachment.mime) }
                                    }
                                    .padding(vertical = 7.dp),
                                verticalAlignment = Alignment.CenterVertically,
                            ) {
                                AttachmentThumbnail(attachment, file, size = 48)
                                Spacer(Modifier.width(10.dp))
                                Column(Modifier.weight(1f)) {
                                    Text(
                                        attachment.name,
                                        style = MaterialTheme.typography.bodyMedium,
                                        maxLines = 1,
                                        overflow = TextOverflow.Ellipsis,
                                    )
                                    Text(
                                        "${attachment.mime} · ${humanSize(attachment.size)}",
                                        style = MaterialTheme.typography.labelMedium,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    )
                                }
                                Icon(
                                    Icons.Outlined.OpenInNew,
                                    null,
                                    tint = MaterialTheme.colorScheme.onSurfaceVariant,
                                    modifier = Modifier.size(18.dp),
                                )
                                Spacer(Modifier.width(6.dp))
                                IconButton(
                                    onClick = { vm.removeAttachment(event.id, index) },
                                    modifier = Modifier.size(34.dp),
                                ) {
                                    Icon(
                                        Icons.Filled.DeleteOutline,
                                        "移除附件",
                                        tint = MaterialTheme.colorScheme.error,
                                        modifier = Modifier.size(19.dp),
                                    )
                                }
                            }
                        }
                    }
                }
            }
        } else {
            Column(
                Modifier
                    .fillMaxSize()
                    .verticalScroll(rememberScrollState())
                    .padding(horizontal = 12.dp)
                    .padding(top = 12.dp, bottom = 32.dp),
            ) {
                PageBoard(
                    event = event,
                    attachments = event.attachments,
                    initialLayout = event.pageLayout,
                    resolveFile = vm.repository::resolveAttachment,
                    onSave = { vm.updatePageLayout(event.id, it) },
                )
            }
        }
    }

    if (confirmDelete) {
        ConfirmDialog(
            title = "删除这条记录？",
            text = "「${event.title}」将被移除，附件文件会保留以便撤销。",
            onConfirm = {
                vm.delete(event.id)
                onBack()
            },
            onDismiss = { confirmDelete = false },
        )
    }
    preview?.let { attachment ->
        ImagePreviewDialog(
            attachment = attachment,
            file = vm.repository.resolveAttachment(attachment),
            onDismiss = { preview = null },
            onOpen = {
                vm.repository.resolveAttachment(attachment)?.let {
                    openAttachment(context, it, attachment.mime)
                }
            },
        )
    }
}

@Composable
private fun MetaRow(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    label: String,
    value: String,
    color: Color = MaterialTheme.colorScheme.onSurface,
) {
    Row(
        Modifier
            .fillMaxWidth()
            .padding(vertical = 7.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(
            icon,
            null,
            tint = MaterialTheme.colorScheme.primary,
            modifier = Modifier.size(18.dp),
        )
        Spacer(Modifier.width(10.dp))
        Text(
            label,
            style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.width(70.dp),
        )
        Text(
            value,
            style = MaterialTheme.typography.bodyMedium,
            color = color,
            modifier = Modifier.weight(1f),
        )
    }
}

@Composable
private fun StatusPill(text: String, color: Color) {
    Surface(shape = RoundedCornerShape(9.dp), color = color.copy(alpha = 0.12f)) {
        Text(
            text,
            style = MaterialTheme.typography.labelMedium,
            color = color,
            fontWeight = FontWeight.SemiBold,
            modifier = Modifier.padding(horizontal = 8.dp, vertical = 3.dp),
        )
    }
}

@Composable
private fun ImagePreviewDialog(
    attachment: Attachment,
    file: File?,
    onDismiss: () -> Unit,
    onOpen: () -> Unit,
) {
    var bitmap by remember(file?.path) { mutableStateOf<ImageBitmap?>(null) }
    androidx.compose.runtime.LaunchedEffect(file?.path) {
        bitmap = file?.let { decodeImage(it, 1600) }
    }
    Dialog(onDismissRequest = onDismiss) {
        Surface(
            shape = RoundedCornerShape(24.dp),
            color = MaterialTheme.colorScheme.surface,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Column {
                Row(
                    Modifier.padding(start = 18.dp, top = 8.dp, end = 8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        attachment.name,
                        style = MaterialTheme.typography.titleMedium,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.weight(1f),
                    )
                    IconButton(onClick = onDismiss) { Icon(Icons.Outlined.Close, "关闭") }
                }
                val bmp = bitmap
                if (bmp != null) {
                    Image(
                        bitmap = bmp,
                        contentDescription = attachment.name,
                        contentScale = ContentScale.Fit,
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(360.dp)
                            .background(MaterialTheme.colorScheme.surfaceVariant),
                    )
                }
                Row(
                    Modifier.fillMaxWidth().padding(10.dp),
                    horizontalArrangement = Arrangement.End,
                ) {
                    TextButton(onClick = onOpen) { Text("用其他应用打开") }
                    TextButton(onClick = onDismiss) { Text("关闭") }
                }
            }
        }
    }
}

private fun readUri(context: Context, uri: Uri): Triple<String, ByteArray, String?>? = runCatching {
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
    Triple(name, bytes, resolver.getType(uri) ?: guessMime(name))
}.getOrNull()

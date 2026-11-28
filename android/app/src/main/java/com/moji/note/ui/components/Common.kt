package com.moji.note.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowDownward
import androidx.compose.material.icons.filled.ArrowUpward
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.DeleteOutline
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.PushPin
import androidx.compose.material.icons.filled.RadioButtonUnchecked
import androidx.compose.material.icons.filled.Schedule
import androidx.compose.material.icons.outlined.AttachFile
import androidx.compose.material.icons.outlined.NotificationsActive
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Color
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.moji.note.data.MojiEvent
import com.moji.note.data.MojiTime
import com.moji.note.ui.theme.parseColor

@Composable
fun TaskCard(
    event: MojiEvent,
    now: Long,
    onClick: () -> Unit,
    onToggle: () -> Unit,
    onEdit: () -> Unit,
    onDuplicate: () -> Unit,
    onMoveUp: () -> Unit,
    onMoveDown: () -> Unit,
    onPin: () -> Unit,
    onDelete: () -> Unit,
) {
    var menuOpen by remember { mutableStateOf(false) }
    val scheme = MaterialTheme.colorScheme
    val overdue = event.isOverdue(now)
    val today = !overdue && !event.done && event.isDueToday(now)
    val stripe = when {
        event.done -> scheme.outline.copy(alpha = 0.5f)
        overdue -> scheme.error
        today -> scheme.tertiary
        event.color.isNotBlank() -> parseColor(event.color, scheme.primary)
        else -> scheme.primary
    }
    Surface(
        onClick = onClick,
        shape = RoundedCornerShape(22.dp),
        color = if (event.done) scheme.surfaceContainerLow else scheme.surface,
        tonalElevation = 0.dp,
        shadowElevation = 1.dp,
        modifier = Modifier.fillMaxWidth(),
    ) {
        Row(Modifier.height(IntrinsicSize.Min)) {
            Box(
                Modifier
                    .width(5.dp)
                    .fillMaxHeight()
                    .background(stripe),
            )
            Column(Modifier.weight(1f).padding(start = 14.dp, end = 6.dp, top = 12.dp, bottom = 14.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    IconButton(onClick = onToggle, modifier = Modifier.size(34.dp)) {
                        Icon(
                            imageVector = if (event.done) Icons.Filled.CheckCircle
                            else Icons.Filled.RadioButtonUnchecked,
                            contentDescription = if (event.done) "标记为未完成" else "标记为完成",
                            tint = if (event.done) scheme.primary else scheme.outline,
                            modifier = Modifier.size(23.dp),
                        )
                    }
                    Spacer(Modifier.width(6.dp))
                    Column(Modifier.weight(1f)) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            if (event.icon.isNotBlank()) {
                                Text(event.icon, style = MaterialTheme.typography.titleMedium)
                                Spacer(Modifier.width(6.dp))
                            }
                            Text(
                                text = event.title,
                                style = MaterialTheme.typography.titleMedium,
                                fontWeight = FontWeight.SemiBold,
                                textDecoration = if (event.done) TextDecoration.LineThrough else null,
                                color = if (event.done) scheme.onSurfaceVariant else scheme.onSurface,
                                maxLines = 2,
                                overflow = TextOverflow.Ellipsis,
                            )
                            Spacer(Modifier.weight(1f))
                            StatusBadge(
                                text = when {
                                    event.done -> "已完成"
                                    overdue -> "逾期"
                                    today -> "今天"
                                    else -> "P${event.priority}"
                                },
                                color = when {
                                    event.done -> scheme.onSurfaceVariant
                                    overdue -> scheme.error
                                    today -> scheme.tertiary
                                    else -> scheme.primary
                                },
                            )
                            Box {
                                IconButton(
                                    onClick = { menuOpen = true },
                                    modifier = Modifier.size(30.dp),
                                ) {
                                    Icon(
                                        Icons.Filled.MoreVert,
                                        contentDescription = "更多操作",
                                        tint = scheme.onSurfaceVariant,
                                        modifier = Modifier.size(19.dp),
                                    )
                                }
                                DropdownMenu(expanded = menuOpen, onDismissRequest = { menuOpen = false }) {
                                    DropdownMenuItem(
                                        text = { Text("编辑") },
                                        leadingIcon = { Icon(Icons.Filled.Edit, null) },
                                        onClick = { menuOpen = false; onEdit() },
                                    )
                                    DropdownMenuItem(
                                        text = { Text("置顶") },
                                        leadingIcon = { Icon(Icons.Filled.PushPin, null) },
                                        onClick = { menuOpen = false; onPin() },
                                    )
                                    DropdownMenuItem(
                                        text = { Text("上移") },
                                        leadingIcon = { Icon(Icons.Filled.ArrowUpward, null) },
                                        onClick = { menuOpen = false; onMoveUp() },
                                    )
                                    DropdownMenuItem(
                                        text = { Text("下移") },
                                        leadingIcon = { Icon(Icons.Filled.ArrowDownward, null) },
                                        onClick = { menuOpen = false; onMoveDown() },
                                    )
                                    DropdownMenuItem(
                                        text = { Text("创建副本") },
                                        leadingIcon = { Icon(Icons.Filled.ContentCopy, null) },
                                        onClick = { menuOpen = false; onDuplicate() },
                                    )
                                    DropdownMenuItem(
                                        text = { Text("删除", color = scheme.error) },
                                        leadingIcon = {
                                            Icon(Icons.Filled.DeleteOutline, null, tint = scheme.error)
                                        },
                                        onClick = { menuOpen = false; onDelete() },
                                    )
                                }
                            }
                        }
                        if (event.note.isNotBlank()) {
                            Spacer(Modifier.height(3.dp))
                            Text(
                                text = event.note.replace('\n', ' ').trim(),
                                style = MaterialTheme.typography.bodyMedium,
                                color = scheme.onSurfaceVariant,
                                maxLines = 2,
                                overflow = TextOverflow.Ellipsis,
                            )
                        }
                        Spacer(Modifier.height(8.dp))
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Icon(
                                    Icons.Filled.Schedule,
                                    contentDescription = null,
                                    tint = when {
                                        overdue -> scheme.error
                                        today -> scheme.tertiary
                                        else -> scheme.onSurfaceVariant
                                    },
                                    modifier = Modifier.size(14.dp),
                                )
                                Spacer(Modifier.width(3.dp))
                                Text(
                                    text = event.dueText(now),
                                    style = MaterialTheme.typography.labelMedium,
                                    color = when {
                                        overdue -> scheme.error
                                        today -> scheme.tertiary
                                        else -> scheme.onSurfaceVariant
                                    },
                                )
                            }
                            if (event.remindBefore > 0) {
                                Icon(
                                    Icons.Outlined.NotificationsActive,
                                    contentDescription = null,
                                    tint = scheme.onSurfaceVariant,
                                    modifier = Modifier.size(14.dp),
                                )
                            }
                            if (event.attachments.isNotEmpty()) {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    Icon(
                                        Icons.Outlined.AttachFile,
                                        contentDescription = null,
                                        tint = scheme.onSurfaceVariant,
                                        modifier = Modifier.size(14.dp),
                                    )
                                    Text(
                                        "${event.attachments.size}",
                                        style = MaterialTheme.typography.labelMedium,
                                        color = scheme.onSurfaceVariant,
                                    )
                                }
                            }
                            val shownTags = event.tags.take(3)
                            for (tag in shownTags) {
                                Surface(
                                    shape = RoundedCornerShape(8.dp),
                                    color = scheme.secondaryContainer.copy(alpha = 0.7f),
                                ) {
                                    Text(
                                        "#$tag",
                                        style = MaterialTheme.typography.labelMedium,
                                        color = scheme.onSecondaryContainer,
                                        modifier = Modifier.padding(horizontal = 6.dp, vertical = 2.dp),
                                    )
                                }
                            }
                            if (event.tags.size > shownTags.size) {
                                Text(
                                    "+${event.tags.size - shownTags.size}",
                                    style = MaterialTheme.typography.labelMedium,
                                    color = scheme.onSurfaceVariant,
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun StatusBadge(text: String, color: Color) {
    Surface(
        shape = RoundedCornerShape(8.dp),
        color = color.copy(alpha = 0.12f),
        modifier = Modifier.padding(horizontal = 4.dp),
    ) {
        Text(
            text = text,
            style = MaterialTheme.typography.labelMedium,
            fontWeight = FontWeight.SemiBold,
            color = color,
            modifier = Modifier.padding(horizontal = 7.dp, vertical = 2.dp),
        )
    }
}

@Composable
fun EmptyState(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    title: String,
    subtitle: String,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier.fillMaxWidth().padding(vertical = 64.dp, horizontal = 32.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Box(
            Modifier
                .size(64.dp)
                .clip(CircleShape)
                .background(MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.6f)),
            contentAlignment = Alignment.Center,
        ) {
            Icon(
                icon,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.primary,
                modifier = Modifier.size(30.dp),
            )
        }
        Spacer(Modifier.height(16.dp))
        Text(title, style = MaterialTheme.typography.titleMedium)
        Spacer(Modifier.height(6.dp))
        Text(
            subtitle,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            textAlign = androidx.compose.ui.text.style.TextAlign.Center,
        )
    }
}

@Composable
fun ConfirmDialog(
    title: String,
    text: String,
    confirmLabel: String = "删除",
    destructive: Boolean = true,
    onConfirm: () -> Unit,
    onDismiss: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(title) },
        text = { Text(text) },
        confirmButton = {
            TextButton(onClick = { onConfirm(); onDismiss() }) {
                Text(
                    confirmLabel,
                    color = if (destructive) MaterialTheme.colorScheme.error
                    else MaterialTheme.colorScheme.primary,
                )
            }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("取消") } },
    )
}

@Composable
fun SectionHeader(text: String, trailing: String? = null, onTrailing: (() -> Unit)? = null) {
    Row(
        Modifier.fillMaxWidth().padding(top = 18.dp, bottom = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text,
            style = MaterialTheme.typography.titleMedium,
            fontWeight = FontWeight.SemiBold,
            color = MaterialTheme.colorScheme.onSurface,
        )
        Spacer(Modifier.weight(1f))
        if (trailing != null) {
            Text(
                trailing,
                style = MaterialTheme.typography.labelLarge,
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier
                    .clip(RoundedCornerShape(8.dp))
                    .clickable(enabled = onTrailing != null) { onTrailing?.invoke() }
                    .padding(horizontal = 8.dp, vertical = 4.dp),
            )
        }
    }
}

fun friendlyTime(now: Long): String {
    val cal = java.util.Calendar.getInstance()
    cal.timeInMillis = now * 1000L
    val hour = cal.get(java.util.Calendar.HOUR_OF_DAY)
    val greeting = when (hour) {
        in 5..10 -> "早上好"
        in 11..13 -> "中午好"
        in 14..17 -> "下午好"
        in 18..22 -> "晚上好"
        else -> "夜深了"
    }
    return greeting
}

fun todayChinese(now: Long): String {
    val cal = java.util.Calendar.getInstance()
    cal.timeInMillis = now * 1000L
    val months = arrayOf("一", "二", "三", "四", "五", "六", "七", "八", "九", "十", "十一", "十二")
    val month = months[cal.get(java.util.Calendar.MONTH)]
    val day = cal.get(java.util.Calendar.DAY_OF_MONTH)
    return "${month}月${day}日 · 周${MojiTime.weekdayCn(now)}"
}

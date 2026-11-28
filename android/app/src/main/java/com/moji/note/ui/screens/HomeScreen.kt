package com.moji.note.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CloudSync
import androidx.compose.material.icons.filled.Redo
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Undo
import androidx.compose.material.icons.outlined.CheckCircle
import androidx.compose.material.icons.outlined.FilterAltOff
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilterChipDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.moji.note.data.MojiTime
import com.moji.note.ui.MojiViewModel
import com.moji.note.ui.TaskFilter
import com.moji.note.ui.components.BrandLogo
import com.moji.note.ui.components.BrandTransform
import com.moji.note.ui.components.EmptyState
import com.moji.note.ui.components.TaskCard
import com.moji.note.ui.components.friendlyTime
import com.moji.note.ui.components.todayChinese
import kotlinx.coroutines.delay

@Composable
fun HomeScreen(
    vm: MojiViewModel,
    onOpen: (String) -> Unit,
    onEdit: (String) -> Unit,
    onCreate: () -> Unit,
    onOpenSearch: () -> Unit,
    onOpenSettings: () -> Unit,
    onRequestSync: () -> Unit,
) {
    val events by vm.visibleEvents.collectAsStateWithLifecycle()
    val allEvents by vm.events.collectAsStateWithLifecycle()
    val stats by vm.stats.collectAsStateWithLifecycle()
    val filter by vm.filter.collectAsStateWithLifecycle()
    val query by vm.query.collectAsStateWithLifecycle()
    val tags by vm.tags.collectAsStateWithLifecycle()
    val selectedTag by vm.selectedTag.collectAsStateWithLifecycle()
    val canUndo by vm.canUndo.collectAsStateWithLifecycle()
    val canRedo by vm.canRedo.collectAsStateWithLifecycle()
    val settings by vm.settings.collectAsStateWithLifecycle()
    var now by remember { mutableLongStateOf(MojiTime.nowSeconds()) }
    LaunchedEffect(Unit) {
        while (true) {
            delay(30_000)
            now = MojiTime.nowSeconds()
        }
    }
    val scheme = MaterialTheme.colorScheme
    val headerText = scheme.onPrimary
    val gradient = Brush.linearGradient(
        listOf(
            scheme.primary,
            androidx.compose.ui.graphics.lerp(scheme.primary, scheme.tertiary, 0.55f),
        ),
    )
    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(bottom = 108.dp),
    ) {
        item {
            Box(
                Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(bottomStart = 30.dp, bottomEnd = 30.dp))
                    .background(gradient)
                    .statusBarsPadding()
                    .padding(start = 20.dp, end = 12.dp, top = 8.dp, bottom = 20.dp),
            ) {
                Column {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        val logoFile = remember(settings.brandLogo, settings.brandRevision) {
                            vm.brandLogoFile()
                        }
                        if (logoFile != null) {
                            BrandLogo(
                                file = logoFile,
                                transform = remember(settings.brandTransform) {
                                    BrandTransform.parse(settings.brandTransform)
                                },
                                size = 50.dp,
                            )
                            Spacer(Modifier.width(12.dp))
                        } else {
                            Box(
                                Modifier
                                    .size(50.dp)
                                    .clip(RoundedCornerShape(16.dp))
                                    .background(headerText.copy(alpha = 0.18f)),
                                contentAlignment = Alignment.Center,
                            ) {
                                Text(
                                    settings.appName.trim().take(1).ifEmpty { "墨" },
                                    fontSize = 24.sp,
                                    fontWeight = FontWeight.Bold,
                                    color = headerText,
                                )
                            }
                            Spacer(Modifier.width(12.dp))
                        }
                        Column(Modifier.weight(1f)) {
                            if (settings.appName.isNotBlank()) {
                                Text(
                                    settings.appName,
                                    style = MaterialTheme.typography.headlineMedium,
                                    color = headerText,
                                    fontWeight = FontWeight.Bold,
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis,
                                )
                            }
                            if (settings.appSubtitle.isNotBlank()) {
                                Text(
                                    settings.appSubtitle,
                                    style = MaterialTheme.typography.labelMedium,
                                    color = headerText.copy(alpha = 0.72f),
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis,
                                )
                            }
                        }
                        if (canUndo) {
                            IconButton(onClick = { vm.undo() }) {
                                Icon(Icons.Filled.Undo, "撤销", tint = headerText)
                            }
                        }
                        if (canRedo) {
                            IconButton(onClick = { vm.redo() }) {
                                Icon(Icons.Filled.Redo, "重做", tint = headerText)
                            }
                        }
                        IconButton(onClick = onRequestSync) {
                            Icon(Icons.Filled.CloudSync, "同步", tint = headerText)
                        }
                        IconButton(onClick = onOpenSettings) {
                            Icon(Icons.Filled.Settings, "设置", tint = headerText)
                        }
                    }
                    Spacer(Modifier.height(16.dp))
                    Text(
                        "${friendlyTime(now)}，${todayChinese(now)}",
                        style = MaterialTheme.typography.bodyMedium,
                        color = headerText.copy(alpha = 0.8f),
                    )
                    Spacer(Modifier.height(14.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        HeaderStat("今日安排", stats.today.toString(), Modifier.weight(1f))
                        HeaderStat("待办", stats.pending.toString(), Modifier.weight(1f))
                        HeaderStat("逾期", stats.overdue.toString(), Modifier.weight(1f))
                        HeaderStat("完成率", "${stats.rate}%", Modifier.weight(1f))
                    }
                    Spacer(Modifier.height(14.dp))
                    LinearProgressIndicator(
                        progress = { if (stats.total == 0) 0f else stats.done.toFloat() / stats.total },
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(6.dp)
                            .clip(RoundedCornerShape(3.dp)),
                        color = headerText,
                        trackColor = headerText.copy(alpha = 0.22f),
                    )
                    Spacer(Modifier.height(6.dp))
                    Text(
                        "已连接：${settings.syncLabel} · 点击右上角云图标同步",
                        style = MaterialTheme.typography.labelMedium,
                        color = headerText.copy(alpha = 0.72f),
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
            }
        }
        item {
            Surface(
                onClick = onOpenSearch,
                shape = RoundedCornerShape(18.dp),
                color = scheme.surface,
                shadowElevation = 1.dp,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, vertical = 14.dp),
            ) {
                Row(
                    Modifier.padding(horizontal = 16.dp, vertical = 13.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Icon(Icons.Filled.Search, null, tint = scheme.onSurfaceVariant, modifier = Modifier.size(20.dp))
                    Spacer(Modifier.width(10.dp))
                    Text(
                        if (query.isBlank()) "搜索标题、备注、标签…" else "搜索：$query",
                        style = MaterialTheme.typography.bodyMedium,
                        color = if (query.isBlank()) scheme.onSurfaceVariant else scheme.onSurface,
                    )
                }
            }
        }
        item {
            LazyRow(
                contentPadding = PaddingValues(horizontal = 16.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                items(TaskFilter.entries.toList()) { item ->
                    FilterChip(
                        selected = filter == item,
                        onClick = { vm.setFilter(item) },
                        label = { Text(item.label) },
                        shape = RoundedCornerShape(12.dp),
                        colors = FilterChipDefaults.filterChipColors(
                            selectedContainerColor = scheme.primary,
                            selectedLabelColor = scheme.onPrimary,
                        ),
                    )
                }
            }
        }
        if (tags.isNotEmpty()) {
            item {
                LazyRow(
                    contentPadding = PaddingValues(horizontal = 16.dp, vertical = 8.dp),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    item {
                        FilterChip(
                            selected = selectedTag == null,
                            onClick = { vm.setTag(null) },
                            label = { Text("全部标签") },
                            shape = RoundedCornerShape(10.dp),
                        )
                    }
                    val sorted = tags.entries.sortedByDescending { it.value }
                    items(sorted) { entry ->
                        FilterChip(
                            selected = selectedTag == entry.key,
                            onClick = { vm.setTag(if (selectedTag == entry.key) null else entry.key) },
                            label = { Text("#${entry.key}  ${entry.value}") },
                            shape = RoundedCornerShape(10.dp),
                        )
                    }
                }
            }
        }
        item {
            Row(
                Modifier
                    .fillMaxWidth()
                    .padding(start = 20.dp, end = 16.dp, top = 16.dp, bottom = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    when (filter) {
                        TaskFilter.ALL -> "全部任务"
                        TaskFilter.PENDING -> "待办"
                        TaskFilter.TODAY -> "今日到期"
                        TaskFilter.OVERDUE -> "已逾期"
                        TaskFilter.DONE -> "已完成"
                    },
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold,
                )
                Spacer(Modifier.width(8.dp))
                Surface(
                    shape = RoundedCornerShape(8.dp),
                    color = scheme.surfaceVariant,
                ) {
                    Text(
                        "${events.size}",
                        style = MaterialTheme.typography.labelMedium,
                        color = scheme.onSurfaceVariant,
                        modifier = Modifier.padding(horizontal = 7.dp, vertical = 2.dp),
                    )
                }
                Spacer(Modifier.weight(1f))
                if (stats.done > 0) {
                    Text(
                        "清除已完成",
                        style = MaterialTheme.typography.labelLarge,
                        color = scheme.primary,
                        modifier = Modifier
                            .clip(RoundedCornerShape(8.dp))
                            .clickable { vm.clearDone() }
                            .padding(horizontal = 8.dp, vertical = 4.dp),
                    )
                }
            }
        }
        if (events.isEmpty()) {
            item {
                EmptyState(
                    icon = if (query.isNotBlank() || selectedTag != null) Icons.Outlined.FilterAltOff
                    else Icons.Outlined.CheckCircle,
                    title = when {
                        query.isNotBlank() || selectedTag != null -> "没有匹配的记录"
                        filter == TaskFilter.DONE -> "还没有完成的任务"
                        allEvents.isEmpty() -> "开始记录第一件事"
                        else -> "这个分类下暂时没有任务"
                    },
                    subtitle = when {
                        allEvents.isEmpty() -> "点击右下角 + 新建待办，数据会以 PC 兼容的 events.json 保存"
                        query.isNotBlank() || selectedTag != null -> "试试更换关键词或标签筛选"
                        else -> "切换筛选条件看看，或者新建一条待办"
                    },
                )
            }
        }
        items(events, key = { it.id }) { event ->
            TaskCard(
                event = event,
                now = now,
                onClick = { onOpen(event.id) },
                onToggle = { vm.toggleDone(event.id) },
                onEdit = { onEdit(event.id) },
                onDuplicate = { vm.duplicate(event.id) },
                onMoveUp = { vm.moveBy(event.id, -1) },
                onMoveDown = { vm.moveBy(event.id, 1) },
                onPin = { vm.moveTo(event.id, 1) },
                onDelete = { vm.delete(event.id) },
            )
            Spacer(Modifier.height(10.dp))
        }
        if (events.isNotEmpty()) {
            item {
                Text(
                    "共 ${stats.total} 条记录 · 完成 ${stats.done} 条 · 附件 ${stats.attachments} 个",
                    style = MaterialTheme.typography.labelMedium,
                    color = scheme.onSurfaceVariant,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 20.dp, vertical = 18.dp),
                )
            }
        }
    }
}

@Composable
private fun HeaderStat(label: String, value: String, modifier: Modifier = Modifier) {
    val text = MaterialTheme.colorScheme.onPrimary
    Box(
        modifier
            .clip(RoundedCornerShape(18.dp))
            .background(text.copy(alpha = 0.14f))
            .padding(horizontal = 8.dp, vertical = 10.dp),
    ) {
        Column {
            Text(
                value,
                fontSize = 21.sp,
                lineHeight = 25.sp,
                fontWeight = FontWeight.Bold,
                color = text,
                maxLines = 1,
            )
            Text(
                label,
                fontSize = 11.sp,
                lineHeight = 15.sp,
                color = text.copy(alpha = 0.72f),
                maxLines = 1,
            )
        }
    }
}

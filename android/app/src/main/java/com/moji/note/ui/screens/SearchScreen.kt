package com.moji.note.ui.screens

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
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.outlined.SearchOff
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilterChipDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.moji.note.data.MojiTime
import com.moji.note.ui.MojiViewModel
import com.moji.note.ui.TaskFilter
import com.moji.note.ui.components.EmptyState
import com.moji.note.ui.components.TaskCard

@Composable
fun SearchScreen(
    vm: MojiViewModel,
    onOpen: (String) -> Unit,
    onEdit: (String) -> Unit,
) {
    val query by vm.query.collectAsStateWithLifecycle()
    val events by vm.visibleEvents.collectAsStateWithLifecycle()
    val all by vm.events.collectAsStateWithLifecycle()
    val stats by vm.stats.collectAsStateWithLifecycle()
    val filter by vm.filter.collectAsStateWithLifecycle()
    val tags by vm.tags.collectAsStateWithLifecycle()
    val selectedTag by vm.selectedTag.collectAsStateWithLifecycle()
    var now by remember { mutableLongStateOf(MojiTime.nowSeconds()) }
    val scheme = MaterialTheme.colorScheme

    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(bottom = 108.dp),
    ) {
        item {
            Column(
                Modifier
                    .fillMaxWidth()
                    .statusBarsPadding()
                    .padding(horizontal = 16.dp, vertical = 12.dp),
            ) {
                Text(
                    "搜索",
                    style = MaterialTheme.typography.headlineMedium,
                    fontWeight = FontWeight.Bold,
                )
                Text(
                    "在标题、备注和标签中查找，共 ${all.size} 条记录",
                    style = MaterialTheme.typography.bodyMedium,
                    color = scheme.onSurfaceVariant,
                )
                Spacer(Modifier.height(14.dp))
                OutlinedTextField(
                    value = query,
                    onValueChange = vm::setQuery,
                    placeholder = { Text("输入关键词…") },
                    leadingIcon = { Icon(Icons.Filled.Search, null) },
                    trailingIcon = {
                        if (query.isNotEmpty()) {
                            IconButton(onClick = { vm.setQuery("") }) {
                                Icon(Icons.Filled.Close, "清空搜索")
                            }
                        }
                    },
                    singleLine = true,
                    shape = RoundedCornerShape(18.dp),
                    modifier = Modifier.fillMaxWidth(),
                )
                Spacer(Modifier.height(12.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(20.dp)) {
                    StatText("全部", stats.total.toString())
                    StatText("待办", stats.pending.toString())
                    StatText("今天", stats.today.toString())
                    StatText("逾期", stats.overdue.toString())
                    StatText("完成", stats.done.toString())
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
                    items(tags.entries.sortedByDescending { it.value }.toList()) { entry ->
                        FilterChip(
                            selected = selectedTag == entry.key,
                            onClick = { vm.setTag(if (selectedTag == entry.key) null else entry.key) },
                            label = { Text("#${entry.key} ${entry.value}") },
                            shape = RoundedCornerShape(10.dp),
                        )
                    }
                }
            }
        }
        item {
            Row(
                Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 12.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    if (query.isBlank() && selectedTag == null) "全部记录" else "搜索结果",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold,
                )
                Spacer(Modifier.width(8.dp))
                Surface(shape = RoundedCornerShape(8.dp), color = scheme.surfaceVariant) {
                    Text(
                        "${events.size}",
                        style = MaterialTheme.typography.labelMedium,
                        color = scheme.onSurfaceVariant,
                        modifier = Modifier.padding(horizontal = 7.dp, vertical = 2.dp),
                    )
                }
            }
        }
        if (events.isEmpty()) {
            item {
                EmptyState(
                    icon = Icons.Outlined.SearchOff,
                    title = "没有找到匹配的记录",
                    subtitle = "换个关键词，或清除筛选条件再试一次。",
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
    }
}

@Composable
private fun StatText(label: String, value: String) {
    Column {
        Text(
            value,
            style = MaterialTheme.typography.titleMedium,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colorScheme.primary,
        )
        Text(
            label,
            style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

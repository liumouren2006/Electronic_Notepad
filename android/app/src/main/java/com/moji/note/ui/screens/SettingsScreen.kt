package com.moji.note.ui.screens

import android.app.AlarmManager
import android.app.NotificationManager
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.provider.OpenableColumns
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.verticalScroll
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
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Backup
import androidx.compose.material.icons.filled.CloudDownload
import androidx.compose.material.icons.filled.CloudSync
import androidx.compose.material.icons.filled.CloudUpload
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.Download
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.HelpOutline
import androidx.compose.material.icons.filled.Image
import androidx.compose.material.icons.filled.MusicNote
import androidx.compose.material.icons.filled.NotificationsActive
import androidx.compose.material.icons.filled.Palette
import androidx.compose.material.icons.filled.Storage
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material.icons.filled.Upload
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.moji.note.reminder.AlarmPlayer
import com.moji.note.ui.MojiViewModel
import com.moji.note.ui.components.BrandLogo
import com.moji.note.ui.components.BrandTransform
import com.moji.note.ui.theme.AccentOptions
import com.moji.note.ui.theme.ThemeMode
import com.moji.note.ui.theme.parseColor
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import kotlin.math.max

@Composable
fun SettingsScreen(
    vm: MojiViewModel,
    notificationsEnabled: Boolean,
    onRequestNotifications: () -> Unit,
    onOpenCloud: () -> Unit,
) {
    val context = LocalContext.current
    val clipboard = LocalClipboardManager.current
    val scope = rememberCoroutineScope()
    val settings by vm.settings.collectAsStateWithLifecycle()
    val stats by vm.stats.collectAsStateWithLifecycle()
    val busy by vm.busy.collectAsStateWithLifecycle()
    val cloudBusy by vm.cloudBusy.collectAsStateWithLifecycle()
    var exportFormat by rememberSaveable { mutableStateOf("json") }
    var mergeImport by rememberSaveable { mutableStateOf(true) }
    var showBrandDialog by remember { mutableStateOf(false) }
    var previewPlaying by remember { mutableStateOf(false) }
    var permissionTick by remember { mutableIntStateOf(0) }
    val scheme = MaterialTheme.colorScheme
    val lifecycleOwner = LocalLifecycleOwner.current
    DisposableEffect(lifecycleOwner) {
        val observer = LifecycleEventObserver { _, event ->
            if (event == Lifecycle.Event.ON_RESUME) permissionTick++
        }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
    }
    val exactAlarmAllowed = remember(permissionTick) {
        val manager = context.getSystemService(AlarmManager::class.java)
        Build.VERSION.SDK_INT < Build.VERSION_CODES.S || manager == null ||
            manager.canScheduleExactAlarms()
    }
    val fullScreenAllowed = remember(permissionTick) {
        if (Build.VERSION.SDK_INT < 34) true else
            context.getSystemService(NotificationManager::class.java)
                ?.canUseFullScreenIntent() != false
    }

    val folderPicker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocumentTree(),
    ) { uri ->
        if (uri != null) vm.setSyncFolder(uri)
    }
    val importPicker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument(),
    ) { uri ->
        if (uri != null) vm.importFromUri(uri, mergeImport)
    }
    val exportPicker = rememberLauncherForActivityResult(
        ActivityResultContracts.CreateDocument(
            when (exportFormat) {
                "mjnote" -> "application/octet-stream"
                "txt" -> "text/plain"
                "md" -> "text/markdown"
                else -> "application/json"
            },
        ),
    ) { uri ->
        if (uri != null) vm.exportToUri(exportFormat, uri)
    }
    val logoPicker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument(),
    ) { uri ->
        if (uri != null) scope.launch {
            val read = withContext(Dispatchers.IO) { readDocument(context, uri) }
            if (read != null) vm.setBrandLogo(read.first, read.second)
            else vm.showMessagePublic("无法读取这张图片")
        }
    }
    val ringtonePicker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument(),
    ) { uri ->
        if (uri != null) scope.launch {
            val read = withContext(Dispatchers.IO) { readDocument(context, uri) }
            if (read != null) vm.setDefaultRingtone(read.first, read.second)
            else vm.showMessagePublic("无法读取这个音频")
        }
    }

    LaunchedEffect(previewPlaying) {
        if (previewPlaying) {
            delay(5_000)
            AlarmPlayer.stop("preview")
            previewPlaying = false
        }
    }
    val previewPlayingNow by rememberUpdatedState(previewPlaying)
    DisposableEffect(Unit) {
        onDispose {
            // 仅停止试听；真正的闹钟由 AlarmActivity 管理。
            if (previewPlayingNow) AlarmPlayer.stop("preview")
        }
    }

    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(bottom = 110.dp),
    ) {
        item {
            Column(
                Modifier
                    .fillMaxWidth()
                    .statusBarsPadding()
                    .padding(horizontal = 20.dp, vertical = 14.dp),
            ) {
                Text(
                    "设置",
                    style = MaterialTheme.typography.headlineMedium,
                    fontWeight = FontWeight.Bold,
                )
                Text(
                    "外观、品牌、同步、导入导出与提醒",
                    style = MaterialTheme.typography.bodyMedium,
                    color = scheme.onSurfaceVariant,
                )
            }
        }
        item {
            SettingsSection(
                icon = Icons.Filled.Palette,
                title = "外观",
                help = "「动态取色」会跟随系统壁纸（Android 12+）生成配色；关闭时使用下方自选主题色。",
            ) {
                Text("主题", style = MaterialTheme.typography.labelLarge, color = scheme.onSurfaceVariant)
                Spacer(Modifier.height(8.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    ThemeMode.entries.forEach { mode ->
                        FilterChip(
                            selected = settings.themeMode == mode,
                            onClick = { vm.setThemeMode(mode) },
                            label = { Text(mode.label) },
                            shape = RoundedCornerShape(10.dp),
                        )
                    }
                }
                Spacer(Modifier.height(14.dp))
                Text("主题色", style = MaterialTheme.typography.labelLarge, color = scheme.onSurfaceVariant)
                Spacer(Modifier.height(8.dp))
                Row(
                    Modifier.horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(10.dp),
                ) {
                    AccentOptions.forEach { hex ->
                        val selected = settings.accentHex.equals(hex, ignoreCase = true)
                        Box(
                            Modifier
                                .size(34.dp)
                                .clip(CircleShape)
                                .background(parseColor(hex))
                                .then(
                                    if (selected) Modifier.padding(2.dp) else Modifier,
                                )
                                .clickable { vm.setAccent(hex) },
                            contentAlignment = Alignment.Center,
                        ) {
                            if (selected) {
                                Text("✓", color = Color.White, fontSize = 16.sp)
                            }
                        }
                    }
                }
                Spacer(Modifier.height(14.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text("动态取色", style = MaterialTheme.typography.titleMedium)
                        Text(
                            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) "已支持" else "系统版本不支持",
                            style = MaterialTheme.typography.labelMedium,
                            color = scheme.onSurfaceVariant,
                        )
                    }
                    Switch(
                        checked = settings.dynamicColor && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S,
                        enabled = Build.VERSION.SDK_INT >= Build.VERSION_CODES.S,
                        onCheckedChange = vm::setDynamicColor,
                    )
                }
            }
        }
        item {
            var nameText by rememberSaveable(settings.appName) { mutableStateOf(settings.appName) }
            var subtitleText by rememberSaveable(settings.appSubtitle) {
                mutableStateOf(settings.appSubtitle)
            }
            SettingsSection(
                icon = Icons.Filled.Image,
                title = "品牌与标题",
                help = "左上角的文字与图标都可自定义。图片支持 PNG/JPG/GIF，可缩放、拖动平移、" +
                    "拉伸铺满，并裁剪成圆角矩形、圆形、方形或六边形；GIF 会保留动画。" +
                    "文字留空则只显示图片。",
            ) {
                OutlinedTextField(
                    value = nameText,
                    onValueChange = { nameText = it },
                    label = { Text("应用名称") },
                    placeholder = { Text("例如：墨记") },
                    singleLine = true,
                    shape = RoundedCornerShape(12.dp),
                    modifier = Modifier.fillMaxWidth(),
                )
                Spacer(Modifier.height(10.dp))
                OutlinedTextField(
                    value = subtitleText,
                    onValueChange = { subtitleText = it },
                    label = { Text("副标题") },
                    placeholder = { Text("例如：电子记事本") },
                    singleLine = true,
                    shape = RoundedCornerShape(12.dp),
                    modifier = Modifier.fillMaxWidth(),
                )
                Spacer(Modifier.height(8.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(
                        onClick = { vm.setBrand(nameText, subtitleText) },
                        enabled = nameText != settings.appName || subtitleText != settings.appSubtitle,
                        modifier = Modifier.weight(1f),
                    ) { Text("应用标题") }
                    TextButton(onClick = {
                        nameText = "墨记"
                        subtitleText = "电子记事本"
                        vm.setBrand(nameText, subtitleText)
                    }) { Text("恢复默认") }
                }
                Spacer(Modifier.height(14.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    val logoFile = vm.brandLogoFile()
                    if (logoFile != null) {
                        BrandLogo(
                            file = logoFile,
                            transform = remember(settings.brandTransform) {
                                BrandTransform.parse(settings.brandTransform)
                            },
                            size = 64.dp,
                        )
                    } else {
                        Box(
                            Modifier
                                .size(64.dp)
                                .clip(RoundedCornerShape(20.dp))
                                .background(scheme.primaryContainer),
                            contentAlignment = Alignment.Center,
                        ) {
                            Text(
                                nameText.trim().take(1).ifEmpty { "墨" },
                                fontSize = 28.sp,
                                fontWeight = FontWeight.Bold,
                                color = scheme.onPrimaryContainer,
                            )
                        }
                    }
                    Spacer(Modifier.width(14.dp))
                    Column(Modifier.weight(1f)) {
                        Text("Logo 图片", style = MaterialTheme.typography.titleMedium)
                        Text(
                            if (settings.brandLogo.isBlank()) "未设置，使用文字徽标"
                            else settings.brandLogo.substringAfterLast('/'),
                            style = MaterialTheme.typography.labelMedium,
                            color = scheme.onSurfaceVariant,
                        )
                    }
                }
                Spacer(Modifier.height(10.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(
                        onClick = { logoPicker.launch(arrayOf("image/*")) },
                        modifier = Modifier.weight(1f),
                    ) { Text(if (settings.brandLogo.isBlank()) "选择图片" else "更换图片") }
                    OutlinedButton(
                        onClick = { showBrandDialog = true },
                        enabled = settings.brandLogo.isNotBlank(),
                        modifier = Modifier.weight(1f),
                    ) { Text("裁剪与位置") }
                }
                if (settings.brandLogo.isNotBlank()) {
                    TextButton(onClick = { vm.clearBrandLogo() }) { Text("移除 Logo") }
                }
            }
        }
        item {
            SettingsSection(
                icon = Icons.Filled.CloudSync,
                title = "与 PC 端同步",
                help = "选择包含 events.json 的目录（PC 默认位于 %USERPROFILE%\\.moji）。" +
                    "拉取会读取远端并按 updated_at 合并；推送会把手机的 events.json 写回并备份旧文件；" +
                    "双向同步先合并拉取再推送。相同 id 取较新的一方，标题+创建时间相同的记录自动去重，" +
                    "附件按 attachments/<id>/ 结构复制。",
            ) {
                Surface(
                    shape = RoundedCornerShape(14.dp),
                    color = scheme.surfaceContainerLow,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Row(
                        Modifier.padding(horizontal = 14.dp, vertical = 12.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Icon(Icons.Filled.FolderOpen, null, tint = scheme.primary)
                        Spacer(Modifier.width(10.dp))
                        Column(Modifier.weight(1f)) {
                            Text(
                                settings.syncLabel,
                                style = MaterialTheme.typography.bodyMedium,
                                maxLines = 1,
                            )
                            Text(
                                if (settings.syncUri == null) "尚未连接同步目录" else "已授予持久读写权限",
                                style = MaterialTheme.typography.labelMedium,
                                color = scheme.onSurfaceVariant,
                            )
                        }
                        TextButton(onClick = { folderPicker.launch(null) }) {
                            Text(if (settings.syncUri == null) "选择" else "更换")
                        }
                    }
                }
                Spacer(Modifier.height(10.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(
                        onClick = { vm.sync("pull") },
                        enabled = settings.syncUri != null && !busy,
                        modifier = Modifier.weight(1f),
                    ) {
                        Icon(Icons.Filled.CloudDownload, null, modifier = Modifier.size(17.dp))
                        Spacer(Modifier.width(4.dp))
                        Text("拉取")
                    }
                    OutlinedButton(
                        onClick = { vm.sync("push") },
                        enabled = settings.syncUri != null && !busy,
                        modifier = Modifier.weight(1f),
                    ) {
                        Icon(Icons.Filled.CloudUpload, null, modifier = Modifier.size(17.dp))
                        Spacer(Modifier.width(4.dp))
                        Text("推送")
                    }
                }
                Spacer(Modifier.height(8.dp))
                Button(
                    onClick = { vm.sync("two-way") },
                    enabled = settings.syncUri != null && !busy,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Icon(Icons.Filled.CloudSync, null, modifier = Modifier.size(18.dp))
                    Spacer(Modifier.width(6.dp))
                    Text("双向同步")
                }
                if (settings.syncUri != null) {
                    TextButton(onClick = { vm.clearSyncFolder() }) { Text("断开同步目录") }
                }
            }
        }
        item {
            SettingsSection(
                icon = Icons.Filled.Storage,
                title = "数据",
                help = "导入支持 .json / .mjnote / .txt，合并导入会按 id 与「标题+创建时间」去重。" +
                    "导出支持 .json / .mjnote / .txt / .md：.mjnote 与 PC 端格式一致（store ZIP + " +
                    "manifest.json），可携带附件；JSON 可直接替换 PC 的 events.json。",
            ) {
                Row(
                    Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Column(Modifier.weight(1f)) {
                        Text("数据目录", style = MaterialTheme.typography.titleMedium)
                        Text(
                            vm.dataPath(),
                            style = MaterialTheme.typography.labelMedium,
                            fontFamily = FontFamily.Monospace,
                            color = scheme.onSurfaceVariant,
                        )
                    }
                    IconButton(onClick = {
                        clipboard.setText(AnnotatedString(vm.dataPath()))
                        vm.showMessagePublic("路径已复制")
                    }) {
                        Icon(Icons.Filled.ContentCopy, "复制路径", modifier = Modifier.size(19.dp))
                    }
                }
                Spacer(Modifier.height(6.dp))
                Text(
                    "记录 ${stats.total} 条 · 待办 ${stats.pending} 条 · 完成 ${stats.done} 条 · " +
                        "附件 ${stats.attachments} 个 · 自动备份 ${vm.backupCount()} 份",
                    style = MaterialTheme.typography.bodyMedium,
                    color = scheme.onSurfaceVariant,
                )
                Spacer(Modifier.height(12.dp))
                Text("导入", style = MaterialTheme.typography.labelLarge, color = scheme.onSurfaceVariant)
                Spacer(Modifier.height(6.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text("合并导入（去重）", style = MaterialTheme.typography.bodyMedium)
                    }
                    Switch(checked = mergeImport, onCheckedChange = { mergeImport = it })
                }
                Spacer(Modifier.height(6.dp))
                OutlinedButton(
                    onClick = { importPicker.launch(arrayOf("*/*")) },
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Icon(Icons.Filled.Upload, null, modifier = Modifier.size(18.dp))
                    Spacer(Modifier.width(6.dp))
                    Text("选择文件导入")
                }
                Spacer(Modifier.height(14.dp))
                Text("导出", style = MaterialTheme.typography.labelLarge, color = scheme.onSurfaceVariant)
                Spacer(Modifier.height(6.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf(
                        "json" to "JSON",
                        "mjnote" to "MJNOTE",
                        "txt" to "Todo.txt",
                        "md" to "Markdown",
                    ).forEach { (key, label) ->
                        FilterChip(
                            selected = exportFormat == key,
                            onClick = { exportFormat = key },
                            label = { Text(label) },
                            shape = RoundedCornerShape(10.dp),
                        )
                    }
                }
                Spacer(Modifier.height(8.dp))
                Button(
                    onClick = {
                        val (name, _) = vm.suggestedExportName(exportFormat)
                        exportPicker.launch(name)
                    },
                    enabled = !busy,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Icon(Icons.Filled.Download, null, modifier = Modifier.size(18.dp))
                    Spacer(Modifier.width(6.dp))
                    Text("导出为 ${exportFormat.uppercase()}")
                }
            }
        }
        item {
            SettingsSection(
                icon = Icons.Filled.NotificationsActive,
                title = "提醒",
                help = "锁屏/息屏时到期会点亮屏幕并弹出全屏闹钟页，循环播放铃声；" +
                    "设备正在使用时会弹出带系统闹钟铃声的高优先级通知，点击通知进入闹钟页。" +
                    "通知上也可直接「停止铃声」或「查看详情」。提醒使用精确闹钟；如果系统限制精确闹钟，" +
                    "会自动降级为普通闹钟，另有每 15 分钟一次的后台扫描兜底。铃声优先使用该事项的铃声，" +
                    "未设置则使用这里的默认铃声，再没有就用系统闹钟铃声。音量只影响本应用的提醒铃声。",
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text("通知权限", style = MaterialTheme.typography.titleMedium)
                        Text(
                            if (notificationsEnabled) "已允许" else "未允许，提醒无法显示",
                            style = MaterialTheme.typography.labelMedium,
                            color = if (notificationsEnabled) scheme.onSurfaceVariant else scheme.error,
                        )
                    }
                    if (!notificationsEnabled) {
                        OutlinedButton(onClick = onRequestNotifications) { Text("授权") }
                    }
                }
                if (!exactAlarmAllowed) {
                    Spacer(Modifier.height(10.dp))
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Column(Modifier.weight(1f)) {
                            Text("精确闹钟权限", style = MaterialTheme.typography.bodyMedium)
                            Text(
                                "未开启时提醒可能延迟",
                                style = MaterialTheme.typography.labelMedium,
                                color = scheme.error,
                            )
                        }
                        OutlinedButton(onClick = {
                            runCatching {
                                context.startActivity(
                                    Intent("android.settings.REQUEST_SCHEDULE_EXACT_ALARM").apply {
                                        data = Uri.parse("package:${context.packageName}")
                                    },
                                )
                            }
                        }) { Text("去开启") }
                    }
                }
                if (!fullScreenAllowed) {
                    Spacer(Modifier.height(10.dp))
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Column(Modifier.weight(1f)) {
                            Text("全屏闹钟权限", style = MaterialTheme.typography.bodyMedium)
                            Text(
                                "未开启时锁屏只显示通知",
                                style = MaterialTheme.typography.labelMedium,
                                color = scheme.error,
                            )
                        }
                        OutlinedButton(onClick = {
                            val ok = runCatching {
                                context.startActivity(
                                    Intent("android.settings.MANAGE_APP_USE_FULL_SCREEN_INTENT").apply {
                                        data = Uri.parse("package:${context.packageName}")
                                    },
                                )
                            }.isSuccess
                            if (!ok) {
                                runCatching {
                                    context.startActivity(
                                        Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).apply {
                                            putExtra(Settings.EXTRA_APP_PACKAGE, context.packageName)
                                        },
                                    )
                                }
                            }
                        }) { Text("去开启") }
                    }
                }
                Spacer(Modifier.height(14.dp))
                Text("默认铃声", style = MaterialTheme.typography.labelLarge, color = scheme.onSurfaceVariant)
                Spacer(Modifier.height(6.dp))
                Surface(
                    shape = RoundedCornerShape(14.dp),
                    color = scheme.surfaceContainerLow,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Row(
                        Modifier.padding(horizontal = 14.dp, vertical = 12.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Icon(Icons.Filled.MusicNote, null, tint = scheme.primary)
                        Spacer(Modifier.width(10.dp))
                        Text(
                            if (settings.defaultRingtone.isBlank()) "系统闹钟铃声"
                            else settings.defaultRingtone.substringAfterLast('/'),
                            style = MaterialTheme.typography.bodyMedium,
                            maxLines = 1,
                            modifier = Modifier.weight(1f),
                        )
                        if (settings.defaultRingtone.isNotBlank()) {
                            TextButton(onClick = {
                                if (previewPlaying) {
                                    AlarmPlayer.stop("preview")
                                    previewPlaying = false
                                } else if (AlarmPlayer.isPlayingBy("alarm")) {
                                    vm.showMessagePublic("闹钟正在响铃，请先关闭闹钟")
                                } else {
                                    AlarmPlayer.start(
                                        context,
                                        settings.defaultRingtone,
                                        settings.volume,
                                        owner = "preview",
                                    )
                                    previewPlaying = true
                                }
                            }) { Text(if (previewPlaying) "停止" else "试听") }
                        }
                    }
                }
                Spacer(Modifier.height(8.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(
                        onClick = { ringtonePicker.launch(arrayOf("audio/*")) },
                        modifier = Modifier.weight(1f),
                    ) { Text(if (settings.defaultRingtone.isBlank()) "选择铃声" else "更换铃声") }
                    if (settings.defaultRingtone.isNotBlank()) {
                        OutlinedButton(
                            onClick = {
                                AlarmPlayer.stop("preview")
                                previewPlaying = false
                                vm.clearDefaultRingtone()
                            },
                            modifier = Modifier.weight(1f),
                        ) { Text("使用系统铃声") }
                    }
                }
                Spacer(Modifier.height(14.dp))
                var volume by remember(settings.volume) { mutableFloatStateOf(settings.volume.toFloat()) }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        "铃声音量",
                        style = MaterialTheme.typography.labelLarge,
                        color = scheme.onSurfaceVariant,
                        modifier = Modifier.weight(1f),
                    )
                    Text("${volume.toInt()}%", style = MaterialTheme.typography.labelMedium)
                }
                Slider(
                    value = volume,
                    onValueChange = { volume = it },
                    onValueChangeFinished = { vm.setVolume(volume.toInt()) },
                    valueRange = 0f..100f,
                )
            }
        }
        item {
            SettingsSection(
                icon = Icons.Filled.CloudSync,
                title = "云同步",
                help = "登录后可在 PC、Android 与其他设备之间同步事项、Markdown、页面墨迹和附件。" +
                    "不登录也能完整使用本地功能，打开软件不会强制登录。" +
                    "每次同步先拉取云端 .mjnote，按 updated_at 合并，再上传新版本；" +
                    "云端变化较大时会先弹窗询问。服务只在 Tailscale 内网开放。",
            ) {
                if (settings.cloudToken.isBlank()) {
                    Text(
                        "未登录 · 本地功能不受影响",
                        style = MaterialTheme.typography.bodyMedium,
                        color = scheme.onSurfaceVariant,
                    )
                    Spacer(Modifier.height(10.dp))
                    Button(onClick = onOpenCloud, modifier = Modifier.fillMaxWidth()) {
                        Icon(Icons.Filled.CloudSync, null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(6.dp))
                        Text("登录 / 注册邮箱账号")
                    }
                } else {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Column(Modifier.weight(1f)) {
                            Text(
                                settings.cloudEmail,
                                style = MaterialTheme.typography.titleMedium,
                                fontWeight = FontWeight.SemiBold,
                            )
                            Text(
                                if (settings.cloudLastSync > 0) {
                                    "上次同步 " + com.moji.note.data.MojiTime.fullDateTime(
                                        settings.cloudLastSync,
                                    ) + " · 云端 v${settings.cloudLastVersion}"
                                } else {
                                    "尚未完成过云同步"
                                },
                                style = MaterialTheme.typography.labelMedium,
                                color = scheme.onSurfaceVariant,
                            )
                        }
                        TextButton(onClick = onOpenCloud) { Text("管理") }
                    }
                    Spacer(Modifier.height(8.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(
                            onClick = { vm.cloudManualSync() },
                            enabled = !cloudBusy,
                            modifier = Modifier.weight(1f),
                        ) {
                            if (cloudBusy) {
                                CircularProgressIndicator(
                                    modifier = Modifier.size(16.dp),
                                    strokeWidth = 2.dp,
                                    color = scheme.onPrimary,
                                )
                            } else {
                                Icon(Icons.Filled.CloudSync, null, modifier = Modifier.size(17.dp))
                            }
                            Spacer(Modifier.width(5.dp))
                            Text(if (cloudBusy) "同步中…" else "立即同步")
                        }
                        OutlinedButton(
                            onClick = { vm.cloudLogout() },
                            enabled = !cloudBusy,
                            modifier = Modifier.weight(1f),
                        ) { Text("退出登录") }
                    }
                }
            }
        }
        item {
            SettingsSection(
                icon = Icons.Filled.Backup,
                title = "关于",
                help = "与 PC 端共享 events.json（version 1）与 .mjnote（manifest version 2）格式；" +
                    "支持优先链表顺序、Markdown 备注、附件、OneNote 页面 page_layout 墨迹、" +
                    "撤销/重做与自动备份。数据目录布局：events.json · events.json.bak · backups/ · " +
                    "config.json · attachments/<id>/。卸载应用前请先导出 .mjnote 备份。",
            ) {
                Text("墨记 MoJi · Android 1.2.0", style = MaterialTheme.typography.titleMedium)
                Text(
                    "本地优先、与 PC 端共用数据文件的电子记事本。",
                    style = MaterialTheme.typography.bodyMedium,
                    color = scheme.onSurfaceVariant,
                )
            }
        }
    }

    if (showBrandDialog) {
        BrandLogoDialog(
            file = vm.brandLogoFile(),
            current = BrandTransform.parse(settings.brandTransform),
            onPick = { logoPicker.launch(arrayOf("image/*")) },
            onDismiss = { showBrandDialog = false },
            onSave = { transform ->
                vm.setBrandTransform(transform.toJson())
                showBrandDialog = false
            },
        )
    }
}

@Composable
private fun BrandLogoDialog(
    file: File?,
    current: BrandTransform,
    onPick: () -> Unit,
    onDismiss: () -> Unit,
    onSave: (BrandTransform) -> Unit,
) {
    var transform by remember { mutableStateOf(current) }
    var boxPx by remember { mutableIntStateOf(240) }
    Dialog(onDismissRequest = onDismiss) {
        Surface(
            shape = RoundedCornerShape(26.dp),
            color = MaterialTheme.colorScheme.surface,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Column(Modifier.padding(18.dp)) {
                Text(
                    "裁剪与位置",
                    style = MaterialTheme.typography.titleLarge,
                    fontWeight = FontWeight.SemiBold,
                )
                Spacer(Modifier.height(12.dp))
                Box(
                    Modifier
                        .fillMaxWidth()
                        .height(230.dp)
                        .clip(RoundedCornerShape(18.dp))
                        .background(MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.5f)),
                    contentAlignment = Alignment.Center,
                ) {
                    BrandLogo(
                        file = file,
                        transform = transform,
                        size = 210.dp,
                        modifier = Modifier
                            .onSizeChanged { boxPx = max(it.width, it.height).coerceAtLeast(1) }
                            .pointerInput(Unit) {
                                detectDragGestures { change, drag ->
                                    change.consume()
                                    transform = transform.copy(
                                        dx = (transform.dx + drag.x / boxPx).coerceIn(-1f, 1f),
                                        dy = (transform.dy + drag.y / boxPx).coerceIn(-1f, 1f),
                                    )
                                }
                            },
                    )
                }
                Spacer(Modifier.height(8.dp))
                Text(
                    "拖动图片平移；下图滑块缩放；GIF 会按原动画播放。",
                    style = MaterialTheme.typography.labelMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Spacer(Modifier.height(10.dp))
                Row(
                    Modifier.horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    listOf(
                        "rounded" to "圆角",
                        "circle" to "圆形",
                        "square" to "方形",
                        "hexagon" to "六边形",
                    ).forEach { (key, label) ->
                        FilterChip(
                            selected = transform.shape == key,
                            onClick = { transform = transform.copy(shape = key) },
                            label = { Text(label) },
                            shape = RoundedCornerShape(10.dp),
                        )
                    }
                    FilterChip(
                        selected = transform.stretch,
                        onClick = { transform = transform.copy(stretch = !transform.stretch) },
                        label = { Text("拉伸铺满") },
                        shape = RoundedCornerShape(10.dp),
                    )
                }
                Spacer(Modifier.height(6.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text("缩放", style = MaterialTheme.typography.labelMedium)
                    Slider(
                        value = transform.zoom,
                        onValueChange = { transform = transform.copy(zoom = it) },
                        valueRange = 0.5f..4f,
                        modifier = Modifier
                            .weight(1f)
                            .padding(horizontal = 10.dp),
                    )
                    Text("${(transform.zoom * 100).toInt()}%", fontSize = 12.sp)
                }
                Spacer(Modifier.height(6.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    TextButton(onClick = onPick) { Text("更换图片") }
                    TextButton(onClick = {
                        transform = transform.copy(zoom = 1f, dx = 0f, dy = 0f)
                    }) { Text("重置") }
                    Spacer(Modifier.weight(1f))
                    TextButton(onClick = onDismiss) { Text("取消") }
                    Button(onClick = { onSave(transform) }) { Text("保存") }
                }
            }
        }
    }
}

@Composable
private fun SettingsSection(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    title: String,
    help: String? = null,
    content: @Composable () -> Unit,
) {
    var showHelp by remember { mutableStateOf(false) }
    Surface(
        shape = RoundedCornerShape(22.dp),
        color = MaterialTheme.colorScheme.surface,
        shadowElevation = 1.dp,
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 6.dp),
    ) {
        Column(Modifier.padding(16.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Surface(
                    shape = RoundedCornerShape(11.dp),
                    color = MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.7f),
                ) {
                    Icon(
                        icon,
                        null,
                        tint = MaterialTheme.colorScheme.primary,
                        modifier = Modifier.padding(7.dp).size(18.dp),
                    )
                }
                Spacer(Modifier.width(10.dp))
                Text(
                    title,
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold,
                    modifier = Modifier.weight(1f),
                )
                if (help != null) {
                    IconButton(onClick = { showHelp = true }, modifier = Modifier.size(30.dp)) {
                        Icon(
                            Icons.Filled.HelpOutline,
                            "说明",
                            tint = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.size(19.dp),
                        )
                    }
                }
            }
            Spacer(Modifier.height(12.dp))
            content()
        }
    }
    if (showHelp && help != null) {
        AlertDialog(
            onDismissRequest = { showHelp = false },
            title = { Text(title) },
            text = {
                Text(
                    help,
                    style = MaterialTheme.typography.bodyMedium,
                    modifier = Modifier.verticalScroll(rememberScrollState()),
                )
            },
            confirmButton = {
                TextButton(onClick = { showHelp = false }) { Text("知道了") }
            },
        )
    }
}

private fun readDocument(context: Context, uri: Uri): Pair<String, ByteArray>? = runCatching {
    val resolver = context.contentResolver
    var name = "file"
    resolver.query(uri, null, null, null, null)?.use { cursor ->
        if (cursor.moveToFirst()) {
            val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            if (index >= 0) name = cursor.getString(index) ?: name
        }
    }
    val bytes = resolver.openInputStream(uri)?.use { it.readBytes() }
        ?: throw IllegalStateException("无法读取文件")
    name to bytes
}.getOrNull()

package com.moji.note

import android.Manifest
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.consumeWindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.outlined.Checklist
import androidx.compose.material.icons.outlined.Search
import androidx.compose.material.icons.outlined.Settings
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.moji.note.reminder.AlarmPlayer
import com.moji.note.reminder.Notifications
import com.moji.note.ui.MojiViewModel
import com.moji.note.ui.screens.CloudScreen
import com.moji.note.ui.screens.DetailScreen
import com.moji.note.ui.screens.EditorScreen
import com.moji.note.ui.screens.HomeScreen
import com.moji.note.ui.screens.SearchScreen
import com.moji.note.ui.screens.SettingsScreen
import com.moji.note.ui.theme.MojiTheme
import kotlinx.coroutines.delay

class MainActivity : ComponentActivity() {
    private val vm: MojiViewModel by viewModels()
    private val pendingEventId = mutableStateOf<String?>(null)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        pendingEventId.value = intent?.getStringExtra(Notifications.EXTRA_EVENT_ID)
        setContent { MojiRoot(vm, pendingEventId) }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        pendingEventId.value = intent.getStringExtra(Notifications.EXTRA_EVENT_ID)
    }
}

@Composable
private fun MojiRoot(vm: MojiViewModel, pendingEventId: MutableState<String?>) {
    val settings by vm.settings.collectAsStateWithLifecycle()
    val cloudPrompt by vm.cloudPrompt.collectAsStateWithLifecycle()
    val context = LocalContext.current
    val lifecycleOwner = LocalLifecycleOwner.current
    val snackbarHostState = remember { SnackbarHostState() }

    var tab by rememberSaveable { mutableStateOf("home") }
    var detailId by rememberSaveable { mutableStateOf<String?>(null) }
    var editorOpen by rememberSaveable { mutableStateOf(false) }
    var editorId by rememberSaveable { mutableStateOf<String?>(null) }
    var cloudOpen by rememberSaveable { mutableStateOf(false) }
    var notificationsEnabled by remember { mutableStateOf(Notifications.canPost(context)) }

    val permissionLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestPermission(),
    ) { granted ->
        notificationsEnabled = granted || Notifications.canPost(context)
        if (!granted) vm.showMessagePublic("未授予通知权限，到期提醒将无法显示")
    }

    fun requestNotificationPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS) !=
            android.content.pm.PackageManager.PERMISSION_GRANTED
        ) {
            permissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
        } else if (!Notifications.canPost(context)) {
            runCatching {
                context.startActivity(
                    Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).apply {
                        putExtra(Settings.EXTRA_APP_PACKAGE, context.packageName)
                    },
                )
            }
        }
    }

    LaunchedEffect(Unit) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU && !notificationsEnabled) {
            permissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
        }
    }
    LaunchedEffect(Unit) {
        delay(1800)
        vm.cloudStartupCheck()
    }
    DisposableEffect(lifecycleOwner) {
        val observer = LifecycleEventObserver { _, event ->
            if (event == Lifecycle.Event.ON_RESUME) {
                notificationsEnabled = Notifications.canPost(context)
                // 从全屏闹钟页返回时，仓库可能已在同一进程里变化。
                vm.publish()
                // 用户主动回到应用时，若闹钟还在后台响，停止它，避免没有可见入口。
                AlarmPlayer.stop("alarm")
            }
        }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
    }

    LaunchedEffect(Unit) {
        vm.messages.collect { message ->
            snackbarHostState.currentSnackbarData?.dismiss()
            snackbarHostState.showSnackbar(message)
        }
    }

    LaunchedEffect(pendingEventId.value) {
        val id = pendingEventId.value
        if (id != null) {
            detailId = id
            pendingEventId.value = null
            // 从通知进入详情后收起提醒，避免残留一条“还在响”的通知。
            runCatching {
                androidx.core.app.NotificationManagerCompat.from(context).cancel(id.hashCode())
            }
        }
    }

    MojiTheme(
        mode = settings.themeMode,
        accentHex = settings.accentHex,
        dynamicColor = settings.dynamicColor,
    ) {
        // 详情/编辑/云端登录等全屏页不在 Scaffold 内，需显式提供内容色，
        // 否则深色模式下默认黑字会与深色背景混在一起。
        CompositionLocalProvider(LocalContentColor provides MaterialTheme.colorScheme.onBackground) {
        Box(
            Modifier
                .fillMaxSize()
                .background(MaterialTheme.colorScheme.background),
        ) {
            when {
                cloudOpen -> CloudScreen(
                    vm = vm,
                    onBack = { cloudOpen = false },
                )
                editorOpen -> EditorScreen(
                    vm = vm,
                    eventId = editorId,
                    onClose = { editorOpen = false },
                    onSaved = { id ->
                        editorOpen = false
                        detailId = id
                    },
                )
                detailId != null -> DetailScreen(
                    vm = vm,
                    eventId = detailId!!,
                    onBack = { detailId = null },
                    onEdit = { id ->
                        editorId = id
                        editorOpen = true
                    },
                )
                else -> {
                    BackHandler(enabled = tab != "home") { tab = "home" }
                    Scaffold(
                        containerColor = MaterialTheme.colorScheme.background,
                        contentWindowInsets = WindowInsets(0, 0, 0, 0),
                        bottomBar = {
                            NavigationBar(containerColor = MaterialTheme.colorScheme.surface) {
                                NavigationBarItem(
                                    selected = tab == "home",
                                    onClick = { tab = "home" },
                                    icon = {
                                        Icon(
                                            if (tab == "home") Icons.Outlined.Checklist
                                            else Icons.Outlined.Checklist,
                                            contentDescription = "任务",
                                        )
                                    },
                                    label = { Text("任务") },
                                )
                                NavigationBarItem(
                                    selected = tab == "search",
                                    onClick = { tab = "search" },
                                    icon = {
                                        Icon(
                                            if (tab == "search") Icons.Filled.Search
                                            else Icons.Outlined.Search,
                                            contentDescription = "搜索",
                                        )
                                    },
                                    label = { Text("搜索") },
                                )
                                NavigationBarItem(
                                    selected = tab == "settings",
                                    onClick = { tab = "settings" },
                                    icon = {
                                        Icon(
                                            if (tab == "settings") Icons.Filled.Settings
                                            else Icons.Outlined.Settings,
                                            contentDescription = "设置",
                                        )
                                    },
                                    label = { Text("设置") },
                                )
                            }
                        },
                        floatingActionButton = {
                            if (tab != "settings") {
                                FloatingActionButton(
                                    onClick = {
                                        editorId = null
                                        editorOpen = true
                                    },
                                    containerColor = MaterialTheme.colorScheme.primary,
                                    contentColor = MaterialTheme.colorScheme.onPrimary,
                                ) {
                                    Icon(Icons.Filled.Add, contentDescription = "新建待办")
                                }
                            }
                        },
                    ) { innerPadding ->
                        Box(
                            Modifier
                                .fillMaxSize()
                                .consumeWindowInsets(innerPadding),
                        ) {
                            when (tab) {
                                "search" -> SearchScreen(
                                    vm = vm,
                                    onOpen = { detailId = it },
                                    onEdit = { id -> editorId = id; editorOpen = true },
                                )
                                "settings" -> SettingsScreen(
                                    vm = vm,
                                    notificationsEnabled = notificationsEnabled,
                                    onRequestNotifications = ::requestNotificationPermission,
                                    onOpenCloud = { cloudOpen = true },
                                )
                                else -> HomeScreen(
                                    vm = vm,
                                    onOpen = { detailId = it },
                                    onEdit = { id -> editorId = id; editorOpen = true },
                                    onCreate = { editorId = null; editorOpen = true },
                                    onOpenSearch = { tab = "search" },
                                    onOpenSettings = { tab = "settings" },
                                    onRequestSync = { cloudOpen = true },
                                )
                            }
                        }
                    }
                }
            }
            SnackbarHost(
                hostState = snackbarHostState,
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .padding(bottom = 96.dp),
            )
        }
        }
        cloudPrompt?.let { prompt ->
            AlertDialog(
                onDismissRequest = { vm.dismissCloudPrompt() },
                title = { Text("云端数据变化较大") },
                text = {
                    Text(
                        "云端新增 ${prompt.diff.remoteOnly} 条、更新 ${prompt.diff.remoteNewer} 条、" +
                            "删除 ${prompt.diff.remoteDeleted} 条，本地独有 ${prompt.diff.localOnly} 条，" +
                            "附件相差 ${prompt.diff.attachmentDelta} 个。\n\n" +
                            "同步会在本地合并，同一条记录取更新时间较新的一方。是否现在同步？",
                    )
                },
                confirmButton = {
                    TextButton(onClick = { vm.confirmCloudSync() }) {
                        Text("立即同步")
                    }
                },
                dismissButton = {
                    TextButton(onClick = { vm.dismissCloudPrompt() }) {
                        Text("暂不同步")
                    }
                },
            )
        }
    }
}

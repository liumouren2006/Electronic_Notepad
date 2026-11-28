package com.moji.note.ui.screens

import android.app.Activity
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
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
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.CloudDone
import androidx.compose.material.icons.filled.Logout
import androidx.compose.material.icons.filled.Sync
import androidx.compose.material.icons.outlined.AlternateEmail
import androidx.compose.material.icons.outlined.Cloud
import androidx.compose.material.icons.outlined.Dns
import androidx.compose.material.icons.outlined.Lock
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.unit.dp
import androidx.core.view.WindowCompat
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.moji.note.data.DEFAULT_CLOUD_SERVER
import com.moji.note.data.MojiTime
import com.moji.note.ui.MojiViewModel

@Composable
fun CloudScreen(
    vm: MojiViewModel,
    onBack: () -> Unit,
) {
    val settings by vm.settings.collectAsStateWithLifecycle()
    val busy by vm.cloudBusy.collectAsStateWithLifecycle()
    val scheme = MaterialTheme.colorScheme
    val loggedIn = settings.cloudToken.isNotBlank()
    val view = LocalView.current
    DisposableEffect(view) {
        val activity = view.context as? Activity
        val controller = activity?.let { WindowCompat.getInsetsController(it.window, view) }
        val previous = controller?.isAppearanceLightStatusBars
        if (controller != null) controller.isAppearanceLightStatusBars = false
        onDispose {
            if (controller != null && previous != null) {
                controller.isAppearanceLightStatusBars = previous
            }
        }
    }

    var mode by rememberSaveable { mutableStateOf("login") }
    var server by rememberSaveable { mutableStateOf(settings.cloudServer.ifBlank { DEFAULT_CLOUD_SERVER }) }
    var email by rememberSaveable { mutableStateOf(settings.cloudEmail) }
    var password by rememberSaveable { mutableStateOf("") }
    var displayName by rememberSaveable { mutableStateOf("") }

    BackHandler { onBack() }

    Column(
        Modifier
            .fillMaxSize()
            .background(MaterialTheme.colorScheme.background),
    ) {
        Box(
            Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(bottomStart = 30.dp, bottomEnd = 30.dp))
                .background(
                    Brush.linearGradient(
                        listOf(Color(0xFF4F6EF7), Color(0xFF238EB8)),
                    ),
                )
                .statusBarsPadding()
                .padding(start = 6.dp, end = 20.dp, top = 4.dp, bottom = 22.dp),
        ) {
            Box(
                Modifier
                    .size(150.dp)
                    .align(Alignment.TopEnd)
                    .padding(top = 0.dp),
            ) {
                Box(
                    Modifier
                        .fillMaxSize()
                        .clip(RoundedCornerShape(75.dp))
                        .background(Color.White.copy(alpha = 0.07f)),
                )
            }
            Column {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    IconButton(onClick = onBack) {
                        Icon(Icons.Filled.ArrowBack, "返回", tint = Color.White)
                    }
                    Column(Modifier.weight(1f)) {
                        Text(
                            "云同步",
                            fontSize = 24.sp,
                            fontWeight = FontWeight.Bold,
                            color = Color.White,
                        )
                        Text(
                            "可选功能 · 不登录也能完整使用本地记事",
                            fontSize = 11.5.sp,
                            color = Color.White.copy(alpha = 0.76f),
                        )
                    }
                    Icon(
                        Icons.Outlined.Cloud,
                        null,
                        tint = Color.White.copy(alpha = 0.9f),
                        modifier = Modifier.size(28.dp),
                    )
                }
                Spacer(Modifier.height(16.dp))
                Text(
                    if (loggedIn) "已登录 ${settings.cloudEmail}" else "登录后可在 PC / 手机之间同步",
                    fontSize = 12.5.sp,
                    color = Color.White.copy(alpha = 0.86f),
                )
                Text(
                    settings.cloudServer,
                    fontSize = 10.5.sp,
                    color = Color.White.copy(alpha = 0.62f),
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
        }

        Column(
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .imePadding()
                .padding(horizontal = 16.dp),
        ) {
            Spacer(Modifier.height(16.dp))
            if (loggedIn) {
                Surface(
                    shape = RoundedCornerShape(22.dp),
                    color = MaterialTheme.colorScheme.surface,
                    shadowElevation = 1.dp,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Column(Modifier.padding(18.dp)) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Icon(
                                Icons.Filled.CloudDone,
                                null,
                                tint = scheme.primary,
                                modifier = Modifier.size(30.dp),
                            )
                            Spacer(Modifier.width(12.dp))
                            Column(Modifier.weight(1f)) {
                                Text(
                                    settings.cloudEmail,
                                    style = MaterialTheme.typography.titleMedium,
                                    fontWeight = FontWeight.SemiBold,
                                )
                                Text(
                                    if (settings.cloudLastSync > 0) {
                                        "上次同步 ${MojiTime.fullDateTime(settings.cloudLastSync)}"
                                    } else {
                                        "还没有完成过云同步"
                                    },
                                    style = MaterialTheme.typography.labelMedium,
                                    color = scheme.onSurfaceVariant,
                                )
                            }
                            Surface(
                                shape = RoundedCornerShape(9.dp),
                                color = scheme.primaryContainer.copy(alpha = 0.6f),
                            ) {
                                Text(
                                    "v${settings.cloudLastVersion}",
                                    fontSize = 10.5.sp,
                                    color = scheme.onPrimaryContainer,
                                    modifier = Modifier.padding(horizontal = 8.dp, vertical = 3.dp),
                                )
                            }
                        }
                        Spacer(Modifier.height(16.dp))
                        Button(
                            onClick = { vm.cloudManualSync() },
                            enabled = !busy,
                            modifier = Modifier.fillMaxWidth(),
                        ) {
                            if (busy) {
                                CircularProgressIndicator(
                                    modifier = Modifier.size(17.dp),
                                    strokeWidth = 2.dp,
                                    color = scheme.onPrimary,
                                )
                            } else {
                                Icon(Icons.Filled.Sync, null, modifier = Modifier.size(18.dp))
                            }
                            Spacer(Modifier.width(7.dp))
                            Text(if (busy) "正在同步…" else "立即同步")
                        }
                        Spacer(Modifier.height(8.dp))
                        OutlinedButton(
                            onClick = { vm.cloudLogout() },
                            enabled = !busy,
                            modifier = Modifier.fillMaxWidth(),
                        ) {
                            Icon(Icons.Filled.Logout, null, modifier = Modifier.size(17.dp))
                            Spacer(Modifier.width(6.dp))
                            Text("退出登录")
                        }
                    }
                }
                Spacer(Modifier.height(12.dp))
                InfoCard(
                    "同步范围",
                    "事项、备注 Markdown、页面墨迹、附件与铃声路径都会打包进 .mjnote 快照。" +
                        "同一条记录以 updated_at 较新的一方为准；云端变化较大时会先询问再同步。",
                )
            } else {
                Surface(
                    shape = RoundedCornerShape(22.dp),
                    color = MaterialTheme.colorScheme.surface,
                    shadowElevation = 1.dp,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Column(Modifier.padding(18.dp)) {
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            FilterChip(
                                selected = mode == "login",
                                onClick = { mode = "login" },
                                label = { Text("登录") },
                                shape = RoundedCornerShape(10.dp),
                            )
                            FilterChip(
                                selected = mode == "register",
                                onClick = { mode = "register" },
                                label = { Text("注册") },
                                shape = RoundedCornerShape(10.dp),
                            )
                        }
                        Spacer(Modifier.height(12.dp))
                        OutlinedTextField(
                            value = server,
                            onValueChange = { server = it },
                            label = { Text("服务器地址") },
                            leadingIcon = { Icon(Icons.Outlined.Dns, null) },
                            singleLine = true,
                            shape = RoundedCornerShape(14.dp),
                            modifier = Modifier.fillMaxWidth(),
                        )
                        Spacer(Modifier.height(10.dp))
                        OutlinedTextField(
                            value = email,
                            onValueChange = { email = it },
                            label = { Text("邮箱") },
                            leadingIcon = { Icon(Icons.Outlined.AlternateEmail, null) },
                            singleLine = true,
                            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Email),
                            shape = RoundedCornerShape(14.dp),
                            modifier = Modifier.fillMaxWidth(),
                        )
                        Spacer(Modifier.height(10.dp))
                        OutlinedTextField(
                            value = password,
                            onValueChange = { password = it },
                            label = { Text(if (mode == "register") "密码（至少 8 位）" else "密码") },
                            leadingIcon = { Icon(Icons.Outlined.Lock, null) },
                            singleLine = true,
                            visualTransformation = PasswordVisualTransformation(),
                            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
                            shape = RoundedCornerShape(14.dp),
                            modifier = Modifier.fillMaxWidth(),
                        )
                        if (mode == "register") {
                            Spacer(Modifier.height(10.dp))
                            OutlinedTextField(
                                value = displayName,
                                onValueChange = { displayName = it },
                                label = { Text("昵称（可选）") },
                                singleLine = true,
                                shape = RoundedCornerShape(14.dp),
                                modifier = Modifier.fillMaxWidth(),
                            )
                        }
                        Spacer(Modifier.height(14.dp))
                        Button(
                            onClick = {
                                if (mode == "login") {
                                    vm.cloudLogin(server, email, password)
                                } else {
                                    vm.cloudRegister(server, email, password, displayName)
                                }
                            },
                            enabled = !busy,
                            modifier = Modifier.fillMaxWidth(),
                        ) {
                            if (busy) {
                                CircularProgressIndicator(
                                    modifier = Modifier.size(17.dp),
                                    strokeWidth = 2.dp,
                                    color = scheme.onPrimary,
                                )
                                Spacer(Modifier.width(7.dp))
                                Text("请稍候…")
                            } else {
                                Text(if (mode == "login") "登录并同步" else "注册并登录")
                            }
                        }
                        TextButton(
                            onClick = onBack,
                            modifier = Modifier.align(Alignment.CenterHorizontally),
                        ) { Text("暂不登录，继续使用本地功能") }
                    }
                }
                Spacer(Modifier.height(12.dp))
                InfoCard(
                    "关于邮箱注册",
                    "账号使用邮箱 + 密码，当前不发送邮件验证码。服务只在 Tailscale 内网开放，" +
                        "手机需要与服务器在同一个 tailnet；PC 需安装并登录 Tailscale。",
                )
            }
            Spacer(Modifier.height(30.dp))
        }
    }
}

@Composable
private fun InfoCard(title: String, text: String) {
    Surface(
        shape = RoundedCornerShape(20.dp),
        color = MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.35f),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(Modifier.padding(16.dp)) {
            Text(
                title,
                style = MaterialTheme.typography.titleSmall,
                fontWeight = FontWeight.SemiBold,
                color = MaterialTheme.colorScheme.onPrimaryContainer,
            )
            Spacer(Modifier.height(5.dp))
            Text(
                text,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                lineHeight = 19.sp,
            )
        }
    }
}

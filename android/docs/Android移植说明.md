# 墨记 MoJi · Android 移植说明

本文说明 Android 客户端与 PC 端（`..`，C++17 + Qt6）的对应关系、
数据格式细节、同步算法、UI 设计与测试情况。

---

## 1. 目标与范围

需求概括：

1. 把 PC 端电子记事本移植为 Android 应用 `moji`。
2. UI 遵循 Google Material Design，可参考 Kazumi 的视觉风格。
3. **与 PC 端数据文件完全兼容**，实现多端互通：PC 写的数据 Android 能读，
   Android 写的数据 PC 能读。
4. 功能对齐 PC 端（任务、优先级链表、Markdown、附件、提醒、页面墨迹、导入导出等）。
5. 构建过程单独成文（见 `构建说明.md`）。

实现采用 Kotlin + Jetpack Compose（而不是 WebView / Flutter），原因：

- Compose 直接对应 Material 3 组件与动态取色；
- 文件、通知、闹钟、SAF、WorkManager 均为一等 API，无跨平台桥接损耗；
- 体积小、启动快，便于课程设计演示。

---

## 2. 与 PC 端的分层对应

| PC 端（C++） | Android 端（Kotlin） | 说明 |
| --- | --- | --- |
| `model.hpp/.cpp` `Event` / `Attachment` | `data/Model.kt` `MojiEvent` / `Attachment` | 字段一一对应 |
| `elist.cpp` 优先级链表 | `data/Merge.kt` `reindexed()` + `Repository` 列表操作 | 用有序列表 + 每次重编号实现同一不变量 |
| `store.cpp` 读写/备份 | `Repository.persist()` | 原子写 + `.bak` + `backups/` 轮转 |
| `importer.cpp` | `JsonCodec` / `TodoTxt` / `MarkdownExport` / `MjnoteCodec` | 导入导出逐项移植 |
| `zip.cpp`（store ZIP） | `MjnoteCodec`（`ZipOutputStream` STORED） | method 0，UTF-8 文件名 |
| `repository.cpp` 业务 | `Repository` + `MojiViewModel` | 增删改查、撤销、附件、合并 |
| TUI / Qt GUI | Compose 四个主界面 + 页面板 | 交互按移动端重做 |
| Windows 系统通知 / 响铃 | 通知渠道 + 精确闹钟 + WorkManager | Android 提醒模型 |

**不变量**：任意时刻 `event.priority == 从 1 开始的位置序号`。
Android 端在每次增删、移动、导入、同步后调用 `reindexed()`：
先按 `(priority, created_at)` 稳定排序，再顺序赋 1..N。

---

## 3. 数据格式兼容

### 3.1 `events.json`

与 PC `Store::save_events` 相同的信封：

```json
{
  "app": "moji-note",
  "version": 1,
  "updated_at": "2026-11-24T23:50:00",
  "count": 2,
  "events": [ ... ]
}
```

单条记录字段（与 `Event::to_json` 对齐）：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | string | 8 位十六进制；冲突时重新分配 |
| `title` | string | 必填；空标题在读取时跳过 |
| `note` | string | Markdown 原文 |
| `due` | string | `yyyy-MM-ddTHH:mm:ss`；无期限为 `""` |
| `priority` | int | 1 起始，等于链表位置 |
| `done` / `done_at` | bool / string | `done=false` 时 `done_at` 强制为空 |
| `tags` | string[] | 读取兼容字符串形式（按逗号/空格/# 拆分） |
| `attachments` | object[] | `path/name/mime/size/added_at`，path 相对数据目录 |
| `created_at` / `updated_at` | string | 本地时区、无时区后缀 |
| `remind_before` | int | 提前分钟，非负 |
| `system_notify` | bool | 原样保存（Android 提醒统一走系统通知） |
| `last_reminded_at` | string | 上次提醒时间，去重用 |
| `color` | string | 色条，如 `#5b8def` |
| `icon` | string | emoji / 字符 |
| `page_layout` | string | 页面布局 + 墨迹的 JSON 文本 |

时间解析与 PC `parse_datetime` 对齐：支持 `T` 或空格分隔、`/` 替换 `-`、
结尾 `Z`、`M-d`（补当前年）、纯 `HH:mm`（补当天）、秒可省略；非法值视为空。
写入统一为 `yyyy-MM-ddTHH:mm:ss`（本地时区），空时间写 `""`。

> Android 使用 `org.json`，可能把 `/` 转义为 `\/`。这是合法 JSON，语义相同，
> PC 端 nlohmann/json 可正常读取。其余非 ASCII 字符按 UTF-8 原样输出。

### 3.2 `.mjnote`

与 PC `export_bundle` 一致：

```
manifest.json
files/<event-id>/<index>_<safe-name>
```

- manifest：`app: "moji-note"`, `version: 2`, `exported_at`, `count`, `events`
- 每条 event 的 `attachments` 增加 `"src": "files/..."`，指向 ZIP 成员
- **所有成员使用 STORED（method 0）**：PC 端 `zip_read` 只实现了 store，因此
  Android 生成时显式设置 `method = STORED`、`size`、`compressedSize`、`CRC32`
- 读取兼容 store 与 deflate（`ZipInputStream` 自动处理）
- 导入时：解析 manifest → 为每条事件分配（可能新的）id → 把附件字节写入
  `attachments/<new-id>/` → 重写路径 → 合并去重 → 保存

### 3.3 `todo.txt`

- 导出：`x`、完成日期、`(A)` 优先级、创建日期、标题、`due:yyyy-MM-ddTHH:mm`、`+tag`
- 导入：识别 `(A)`、`due:`、`+tag`、`t:` 忽略、10 位日期（第一个创建、第二个完成）、
  其余拼接为标题；`_` 还原为空格
- 列表顺序即导入顺序，导入后按链表规则重新编号

### 3.4 Markdown 导出

与 PC 相同的分组（进行中 / 已完成）、`### 序号. [x] 标题`、截止、标签、附件、
备注引用块。

### 3.5 `page_layout`

PC 端 `page_view.cpp` 的结构：

```json
{
  "text":  {"x": 40, "y": 40, "z": 10, "w": 680},
  "images": [
    {"path": "attachments/<id>/a.png", "x": 80, "y": 620,
     "w": 260, "h": 180, "rot": 0, "z": 20}
  ],
  "hidden": ["attachments/<id>/b.png"],
  "ink": [
    {"color": "#1f2430", "width": 4,
     "points": [[x1, y1], [x2, y2]]}
  ]
}
```

Android 端：

- 页面尺寸固定 820 × 1200（与 PC 一致），按屏宽等比缩放
- 文本层使用 Markdown 渲染，位于 `text.x/y`、宽度 `text.w`
- 图片层按 `x/y/w/h/rot/z` 绘制，可拖动；`w/h/rot` 原样保留
- 墨迹点使用页面坐标系（0..820, 0..1200），与 PC 完全互通
- `hidden` 原样保留；自动防抖 700 ms 保存

---

## 4. 同步算法

### 4.1 目录约定

同步目录必须包含 `events.json`，附件位于同目录 `attachments/`。
PC 默认数据目录是 `%USERPROFILE%\.moji`。

### 4.2 拉取（merge）

1. 读取远端 `events.json`，解析为 `MojiEvent` 列表
2. 调用 `Merger.syncMerge(local, remote)`：
   - 相同 `id`：比较 `updated_at`（缺失用 `created_at`），较新者胜出
   - 不同 `id` 但 `title + \u001f + created_at` 相同：视为同一条，保留本地
   - 仅远端存在：追加
   - 结果按 `(priority, created_at)` 排序并重编号
3. 对合并结果中本地缺失的附件，按相对路径从远端读取并写入本地
4. 覆盖本地 `events.json`（保留本地备份机制）

### 4.3 推送（push）

1. 把远端旧 `events.json` 写成 `events.json.bak`
2. 写入本地当前 `events.json`
3. 遍历所有附件：远端不存在或大小不同则写入（按需创建 `attachments/<id>/`）

### 4.4 双向同步

先拉取合并，再推送合并后的结果。适合 PC 与手机交替编辑。

### 4.5 覆盖导入

导入 `.json` 时把 `merge` 设为 false：清空本地记录后导入，保留附件文件。
与 PC `Repository::import_events(merge=false)` 一致。

### 4.6 去重指纹

`title + "\u001f" + created_at`，与 PC `import_events` 的指纹完全一致，
因此从 PC 导入多次不会重复。

---

## 4.7 云同步（v1.2.0）

- 服务端 `moji-cloud`（独立 Python 标准库项目，部署在 ECS `dsbot`），只监听 Tailscale
  `100.116.113.16:8789`；客户端默认 `http://dsbot.taild07bbe.ts.net:8789`。
- 账号：邮箱 + 密码注册/登录，Bearer token 30 天；不登录时完全不访问网络。
- 快照：每账号一份 `.mjnote`（manifest v2，含附件），服务端只存快照与元数据，
  解析 manifest 生成 id/updated_at/附件数摘要。
- 客户端流程：
  1. `GET /api/sync/meta`，比较远端摘要和本地 `updated_at`；
  2. 远端新增+更新 >= max(4, 本地数×25%) 或附件差 >= 5 → 启动时 AlertDialog 询问；
     小变化自动同步；用户拒绝后记录 skip 版本；
  3. `GET /api/sync/download` → `Repository.mergeCloudBundle`（同 id 取较新，
     标题+创建时间去重，远端获胜记录附件落盘）；
  4. `Repository.exportBundle()` → `POST /api/sync/upload`，带
     `X-Moji-Base-Version` 和 `X-Moji-Deleted`；409 时重新拉 meta 并重试。
- 删除：SharedPreferences 保存上次同步的 ID 集合，删除后把差集作为墓碑上传；
  拉取时 `applyCloudTombstones` 移除本地未更新的同名记录，避免多端“复活”。
- 令牌/服务器/上次版本存 SharedPreferences；清登录只清令牌，不删本地数据。
- 明文 HTTP 仅用于 Tailscale 内网（WireGuard 加密），Manifest 开了
  `usesCleartextTraffic`；不要把 8789 暴露到公网。
- 云同步页顶部把状态栏图标设为白色；不登录也可以完整使用本地功能。

## 5. 提醒实现

```
AlarmManager.setExactAndAllowWhileIdle(effective_remind_at)
        │  （Android 12+ 无精确闹钟权限时 → setAndAllowWhileIdle）
        ▼
ReminderReceiver
  ├─ repo.find(id)
  ├─ needsReminder(now)?
  │    !done && now >= max(remind_at, snooze_until) && last_reminded < 该提醒点
  ├─ Notifications.show(...)  // 高优先级通知 + setFullScreenIntent
  ├─ repo.markReminded(...)   // 只写 last_reminded_at，不改 updated_at
  └─ scheduleAll(...)         // 重排剩余闹钟（含 snooze）
                │
                ▼
          AlarmActivity（锁屏点亮）
            ├─ AlarmPlayer：MediaPlayer 循环 + 震动
            │    事项铃声 → 默认铃声 → 系统闹钟铃声
            ├─ 静音 / 5 分钟后再提醒（写 snooze_until）/ 完成 / 查看详情
            └─ 取消通知，重排闹钟
```

- 两个通知渠道：
  - `moji-reminders-v2`（静音）：锁屏/息屏且预计会弹全屏页时使用，铃声由 `AlarmActivity` 循环播放
  - `moji-reminders-audible-v1`（有声）：设备解锁时系统通常不启动 full-screen intent，
    改用系统闹钟铃声，保证能听见；首选渠道被用户关闭时自动回退到另一个
- 选择依据：`PowerManager.isInteractive`、`KeyguardManager.isKeyguardLocked`、
  Android 14+ 的 `canUseFullScreenIntent()`
- 通知正文点击直接打开闹钟页，并提供「停止铃声」「查看详情」两个操作；
  `MainActivity.onResume` 也会停掉后台残留的闹钟，避免没有可见入口
- `canDeliver()` 会检查通知权限和渠道是否被关成 IMPORTANCE_NONE，
  未真正送达时不写 `last_reminded_at`，下次扫描还能补发
- 设置页检测 `canScheduleExactAlarms()`（Android 12+）与
  `canUseFullScreenIntent()`（Android 14+），未授权时显示「去开启」
- WorkManager `PeriodicWorkRequest` 每 15 分钟扫描一次，作为兜底
- `BootReceiver` 监听 `BOOT_COMPLETED` / `MY_PACKAGE_REPLACED` / `TIME_SET` /
  `TIMEZONE_CHANGED`，重新安排
- 应用每次数据变更后（增删改、导入、同步）调用 `ReminderScheduler.scheduleAll`
- 已删除 / 已完成 / 已改期的旧闹钟通过 `SharedPreferences` 记录并取消
- 通知点击携带 `event_id`，直接打开详情；全屏意图直接打开闹钟页

与 PC 的差异：PC 有「托盘气泡 + 可选响铃」；Android 用高优先级通知 + 全屏闹钟页。
`system_notify` 字段仍保存，但 Android 端所有到期事项都会提醒
（与 PC 的应用内提醒一致，不受该字段影响）。

---

## 6. UI 设计（Material Design / Kazumi 参考）

界面概念稿见 [`concept-art/`](concept-art/)（HTML/CSS 渲染，非真机截图）。
参考 Kazumi 的视觉语言，但按记事本场景重排：

| 设计点 | 实现 |
| --- | --- |
| 主色 | 默认 `#4f6ef7`，可换 8 色；Android 12+ 可动态取色 |
| 头图 | 主题色 → 第三色的线性渐变，底部 30dp 圆角，展示自定义品牌、问候、日期、四项统计、完成进度 |
| 卡片 | 22dp 大圆角、1dp 阴影、左侧 5dp 状态色条；逾期红、今天青、完成灰 |
| 层级 | Material 3 `surface` / `surfaceContainerLow` / `surfaceVariant` 柔和分层 |
| 导航 | 底部导航（任务 / 搜索 / 设置）+ 悬浮新建按钮 |
| 标签 | 胶囊 Chip，选中时主色填充 |
| 详情 | 顶部标题卡 + 元信息卡 + Markdown 卡（默认渲染，可切源码）+ 附件卡；「页面与墨迹」独立 Tab |
| 编辑 | 卡片分区：备注（编辑/预览）、时间与提醒、优先级/位置、事项铃声、标签、色条、图标、通知、附件 |
| 设置 | 外观 / 品牌与标题 / 同步 / 数据 / 提醒 / 关于 六个分区；长说明收进 `?` 帮助弹窗 |
| 品牌 | 名称、副标题可留空；Logo 图片可缩放、拖动、裁剪为圆角/圆形/方形/六边形，GIF 保留动画 |
| 闹钟 | 全屏渐变页，大号时钟、事项标题、截止时间与备注预览；静音/稍后/完成/详情 |
| 页面反相 | 深色模式下页面纸变深、文字与墨迹取补色，与 PC 端「深色页面反相」一致 |
| 适配 | `enableEdgeToEdge`、`statusBarsPadding`、`imePadding`、手势导航透明底栏 |

交互要点：

- 复选框直接完成/取消完成；卡片点击进详情；右上角菜单提供编辑、置顶、上移、下移、副本、删除
- 返回键：编辑页有未保存修改时先确认；详情页返回列表；非首页 Tab 返回任务页
- 搜索结果与首页共享筛选状态与标签过滤
- 所有失败操作通过 Snackbar 提示，不崩溃

---

## 7. 目录与类职责

见 `README.md` 的「工程结构」。补充几个关键实现：

- `Repository` 内部 `synchronized(lock)`，所有写操作先 `pushUndo()` 再落盘；
  Snackbar 撤销可用 60 步。
- `persist()` 每次写入 `events.json`、`events.json.bak`、`backups/events-<时间>.json`，
  按 `config.json` 的 `keep_backups`（默认 20）轮转。
- `writeAtomic()` 先写同目录临时文件再 `renameTo`，失败时回退覆盖写。
- `resolve(path)` 做 `canonicalFile` 校验，阻止 `../` 路径穿越（与 PC `Store::resolve` 对应）。
- `SyncManager` 通过 `DocumentFile` 遍历 SAF 目录；读写用 `ContentResolver` 流。
- `PageBoard` 页面坐标与物理像素解耦，笔迹/图片位置跨设备一致。

---

## 8. 测试记录

| 用例 | 结果 |
| --- | --- |
| `events.json` 字段往返（含 ringtone / snooze_until）、空时间、PC 旧标签字符串 | 通过 |
| `snooze_until` 推后提醒且不重复、早于基础提醒不提前触发 | 通过 |
| 时间解析：T / 空格 / `/` / Z / 非法值 | 通过 |
| todo.txt 导出→导入：优先级、完成、due、标签空格 | 通过 |
| 导入去重（id 与 title+created_at）、id 冲突重分配 | 通过 |
| 双向同步：updated_at 胜负、远端独有记录、本地保留 | 通过 |
| `.mjnote` STORED、manifest、附件字节往返 | 通过 |
| 优先级重编号不变量 | 通过 |
| 真实 PC `events.json`（7 条，含图片）解析/重写/打包/解包 | 通过（临时 ASCII 目录） |

构建验证：

- `assembleDebug` 成功，APK 17 MB（versionName 1.2.0）
- `lintDebug` 通过（0 error）
- 单元测试 10 项通过（9 项格式测试 + 1 项云同步联调，均在 ASCII 路径副本中运行）
- `aapt2 dump badging` 包名 / SDK / 权限 / 应用名正确
- 尚未在真机或模拟器上做 UI 冒烟（构建机未连接设备、未安装 system image）

---

## 9. 后续可补

1. 页面图片缩放/旋转手势（保留现有 `w/h/rot` 字段，不影响互通）
2. 多选批量完成/删除/移动
3. 自动同步（监听目录变化或定时 WorkManager）
4. 备注内 `{color:...}` 等 PC 扩展标记的渲染
5. 深色模式下的启动窗口背景跟随（目前 Compose 内正确，冷启动瞬间为浅色）
6. 真机 UI 自动化（Compose UI Test / UIAutomator）
7. 导入 `.mjnote` 时对超大附件的流式处理（目前整包读入内存）
8. 真机验证全屏闹钟在锁屏、免打扰、省电模式下的表现（各厂商 ROM 差异较大）

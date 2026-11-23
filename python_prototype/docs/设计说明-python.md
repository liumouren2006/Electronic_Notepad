# 设计说明

本文对应课程设计“设计要求”的四点：数据结构、文件格式、函数原型、容错处理，并补充 UI 与测试说明。

---

## 1. 总体架构

```
        ┌───────────────┐   ┌───────────────┐   ┌───────────────┐
        │   CLI 子命令   │   │   Textual TUI │   │ PySide6 GUI   │
        └───────┬───────┘   └───────┬───────┘   └───────┬───────┘
                └───────────────────┼───────────────────┘
                                    ▼
                         ┌─────────────────────┐
                         │     Repository      │  业务：增删改查、撤销、附件、提醒
                         └──────┬──────┬───────┘
                                │      │
                  ┌─────────────▼─┐  ┌─▼──────────────┐
                  │   EventList   │  │   importer     │
                  │  优先级链表    │  │  json/mjnote/  │
                  └───────┬───────┘  │  todo.txt/md   │
                          │          └────────────────┘
                  ┌───────▼──────────────────────────┐
                  │            Store                 │  文本文件、附件、备份、快照
                  └──────────────────────────────────┘
```

- 核心层 `moji.core` **零第三方依赖**。
- UI 层只调用 Repository，不直接读写文件。
- 所有写操作都会自动落盘；失败抛出 `RepoError`，由 UI 转成提示。

---

## 2. 数据结构

### 2.1 Python `Event`

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | `str` | 8 位十六进制，唯一 |
| `title` | `str` | 必填，非空 |
| `note` | `str` | 多行 Markdown |
| `due` | `datetime \| None` | 本地时间，无期限为 `None` |
| `priority` | `int` | 1 起始，等于链表位置，由 `EventList.reindex()` 维护 |
| `done` / `done_at` | `bool` / `datetime` | 完成状态与完成时间 |
| `tags` | `list[str]` | 去重前的原始标签，输入时清洗 |
| `attachments` | `list[Attachment]` | 相对路径 + MIME + 大小 |
| `created_at` / `updated_at` | `datetime` | 审计字段 |
| `remind_before` | `int` | 提前分钟数，0 = 到点 |
| `last_reminded_at` | `datetime \| None` | 防止同一轮重复提醒 |
| `color` | `str` | GUI 色条颜色，可空 |

`Attachment`：`path`（相对数据目录）、`name`、`mime`、`size`、`added_at`；`is_image/is_audio` 由 MIME 判断。

### 2.2 优先级链表

```python
class _Node:
    __slots__ = ("event", "next")

class EventList:
    _head: _Node | None
    _size: int
```

核心不变量：

1. 任意时刻 `event.priority == 该节点从 1 开始的位置`；
2. 插入到 `pos` 后，原 `pos..n` 全部 +1；
3. 删除后，后续节点整体 -1；
4. 序列化时按链表顺序输出，读取后按 `(priority, created_at)` 稳定排序再 `reindex()`。

复杂度（n 为事件数）：

| 操作 | 复杂度 | 说明 |
| --- | --- | --- |
| 追加 | O(n) | 找到尾节点；n 通常 < 1000 |
| 按 id 查找 | O(n) | 简单链表，符合题目“结构数组/简单链表” |
| 插入到 pos | O(n) + O(n) 重编号 | 指针插入 + 统一维护序号 |
| 删除 | O(n) + O(n) 重编号 | |
| 移动 | O(n) | 摘链 + 插入 |

选择链表而不是 Python `list` 的原因：直接对应课程要求的数据结构；优先级语义（位置）与链式插入天然一致；也便于用 C 版做同构实现。

### 2.3 C 版结构

```c
typedef struct Event {
    int id;
    int priority;
    int done;
    char title[128];
    char note[512];
    char due[20];       /* "YYYY-MM-DD HH:MM" */
    char created[20];
    struct Event *next;
} Event;
```

---

## 3. 文件格式

### 3.1 主数据 `events.json`

UTF-8、缩进 2 空格、按优先级排序：

```json
{
  "app": "moji-note",
  "version": 1,
  "updated_at": "2026-11-23T12:30:00",
  "count": 2,
  "events": [
    {
      "id": "59535cd3",
      "title": "提交课程设计报告",
      "note": "## 清单\n- [x] 核心\n- [ ] 日志",
      "due": "2026-11-23T18:00:00",
      "priority": 1,
      "done": false,
      "done_at": "",
      "tags": ["课程", "截止"],
      "attachments": [
        {
          "path": "attachments/59535cd3/cover.svg",
          "name": "cover.svg",
          "mime": "image/svg+xml",
          "size": 1024,
          "added_at": "2026-11-23T12:00:00"
        }
      ],
      "created_at": "2026-11-23T12:00:00",
      "updated_at": "2026-11-23T12:00:00",
      "remind_before": 60,
      "last_reminded_at": "",
      "color": "#5b8def"
    }
  ]
}
```

选择理由：

- 文本可读、可 diff、可手工修复；
- 时间用 ISO 秒级字符串，跨语言；
- 附件不进 JSON，避免 base64 膨胀；
- 写入采用临时文件 + 原子替换。

### 3.2 目录布局

| 路径 | 作用 |
| --- | --- |
| `events.json` | 主数据 |
| `events.json.bak` | 上一次成功保存的完整副本 |
| `backups/events-*.json` | 每次保存的自动备份，保留 20 份 |
| `backups/undo-*.json` | CLI 写操作前的快照（跨进程撤销） |
| `backups/redo-*.json` | CLI 撤销前的快照（重做） |
| `config.json` | 主题、窗口大小、备份数量 |
| `attachments/<event-id>/<file>` | 附件按事件隔离 |

### 3.3 `.mjnote` 打包

ZIP 结构：

```
manifest.json           # 与 events.json 类似，附件额外带 "src" 成员名
files/<event-id>/0_cover.svg
files/<event-id>/1_demo.wav
```

导入流程：

1. 读取 manifest 与 `files/` 到临时目录；
2. 逐条 `Event.from_dict`，跳过损坏记录；
3. 若 merge=True，按 id 或 `(title, created_at)` 去重；
4. 附件通过 `Store.add_attachment` 复制进新数据目录，重写相对路径；
5. 重新编号并保存；
6. 返回 `{added, skipped, source}`。

### 3.4 todo.txt

导出示例：

```
(A) 2026-11-23 提交课程设计报告 due:2026-11-23T18:00 +课程 +截止
x 2026-11-22 2026-11-21 完成服务器巡检 +运维
```

- 优先级 A/B/C… 对应 P1/P2/P3…；
- `due:` 保留到分钟；
- 标签 `+tag`；
- 导入时在任意 token 位置识别 `(A)`，兼容常见 todo.txt 工具。

### 3.5 C 版 `notes.dat`

```
# MOJI-C 1
1|1|2026-11-24 18:00|0|买牛奶|超市采购
2|2|2026-11-25 09:00|1|写报告|课程设计
```

- `|` 分隔 6 个字段；
- 输入时把 `|`、换行替换为空格；
- 读取时字段不足或 id/title 非法的行跳过并告警；
- 保存同样使用 `.tmp` + `rename`。

---

## 4. 函数原型

### 4.1 模型 / 链表

```python
Event(title: str, *, id: str = "", note: str = "", due: datetime | None = None,
      priority: int = 1, done: bool = False, tags: list[str] = ...,
      attachments: list[Attachment] = ..., remind_before: int = 0,
      color: str = "") -> Event
Event.to_dict() -> dict
Event.from_dict(data: dict) -> Event
Event.needs_reminder(at: datetime | None = None) -> bool
Event.is_overdue -> bool
Event.due_today -> bool

EventList(events: list[Event] | None = None)
EventList.append(event: Event) -> Event
EventList.insert_at(pos: int, event: Event) -> Event
EventList.insert_after(anchor_id: str, event: Event) -> Event
EventList.remove(event_id: str) -> Event | None
EventList.move_to(event_id: str, pos: int) -> bool
EventList.move_by(event_id: str, delta: int) -> bool
EventList.reindex() -> None
EventList.find(event_id: str) -> Event | None
EventList.index_of(event_id: str) -> int
```

### 4.2 仓储

```python
Repository(data_dir: str | None = None)
Repository.add(title: str, note: str = "", due=None, tags=None,
               position: int | None = None, remind_before: int = 0,
               color: str = "") -> Event
Repository.insert_after(anchor_id: str, title: str, **kwargs) -> Event
Repository.duplicate(event_id: str) -> Event
Repository.update(event_id: str, **changes) -> Event     # title/note/due/tags/remind_before/color
Repository.set_done(event_id: str, done: bool = True) -> Event
Repository.toggle_done(event_id: str) -> Event
Repository.move_to(event_id: str, position: int) -> Event
Repository.move_by / move_up / move_down
Repository.delete(event_id: str) -> Event
Repository.clear_done() -> int
Repository.all() -> list[Event]
Repository.pending() / finished() / overdue()
Repository.due_on(day: date | str) -> list[Event]
Repository.due_between(start: datetime, end: datetime) -> list[Event]
Repository.today() / upcoming(days=7)
Repository.search(keyword: str) -> list[Event]
Repository.tags() -> dict[str, int]
Repository.stats() -> dict
Repository.due_reminders(at=None) -> list[Event]
Repository.mark_reminded(events, at=None) -> None
Repository.add_attachment(event_id: str, src_path: str) -> Attachment
Repository.remove_attachment(event_id: str, index: int) -> Attachment
Repository.reorder_attachments(event_id: str, rel_paths: list[str]) -> Event
Repository.undo() -> bool / redo() -> bool
Repository.gc_orphans() -> int
```

### 4.3 存储 / 格式

```python
Store(data_dir=None)
Store.load_events() -> list[dict]
Store.save_events(events: list[dict]) -> None
Store.load_config() -> dict
Store.save_config(config: dict) -> None
Store.add_attachment(event_id: str, src) -> Attachment
Store.resolve(att: Attachment | str) -> Path
Store.attachment_exists(att) -> bool
Store.snapshot(tag: str = "undo", keep: int = 20) -> None
Store.restore_last(tag: str = "undo") -> bool
Store.info() -> dict

export_json(repo, path) / import_json(repo, path, merge=True)
export_bundle(repo, path) -> (Path, int) / import_bundle(repo, path, merge=True)
export_todotxt(repo, path) / import_todotxt(repo, path, merge=True)
export_markdown(repo, path)
export_auto(repo, path) / import_auto(repo, path, merge=True)
```

### 4.4 UI / CLI

```python
cli.main(argv: list[str] | None = None) -> int
tui.app.MojiTUI(repo: Repository).run()
gui.app.launch_gui(repo: Repository) -> int
gui.app.MainWindow(repo: Repository)
```

### 4.5 C 版

```c
void  safe_copy(char *dst, int size, const char *src);
void  read_due(char *buf, int size, const char *prompt);
int   valid_due(const char *text, int date_only);
int   list_count(Event *head);
void  list_reindex(Event *head);
Event *list_find(Event *head, int id);
void  list_insert_at(Event **head, Event *ev, int pos);
void  list_unlink(Event **head, Event *target);
int   list_remove(Event **head, int id);
void  list_clear(Event **head);
int   load_file(Event **head, const char *path);
int   save_file(Event *head, const char *path);
void  print_event(const Event *ev);
void  beep_reminder(Event *head);
void  do_add(int insert_mode);
void  do_modify(void);
void  do_delete(void);
void  do_toggle_done(void);
void  do_reposition(void);
void  do_query_date(void);
void  do_search(void);
void  do_stats(void);
```

---

## 5. 关键流程

### 5.1 新增 / 插入

```
UI 收集字段
  → Repository.add(position=...)
      → Event(...)
      → EventList.append 或 insert_at
      → reindex
      → Store.save_events（原子写 + 备份）
  → UI 重新查询并刷新
```

### 5.2 提醒

```
每 30 秒：
  Repository.due_reminders(now)
    → 遍历事件，needs_reminder:
         not done 且 now >= due - remind_before
         且 last_reminded_at 早于本轮提醒点
  → 有结果：响铃 / Toast / 托盘，mark_reminded 落盘
```

启动时额外：

- TUI 显示今日概览并响铃（若有今日安排）；
- GUI 显示“今日安排”Toast；
- C 版统计当天未完成项并 `Beep`。

### 5.3 删除与撤销

- 删除前把当前完整状态压入内存撤销栈（最多 60 步）；
- 删除只移除索引，**不删除附件文件**，保证撤销后仍能打开；
- 需要回收空间时 `gc_orphans()` 清理无引用目录；
- CLI 因进程不共享内存，改用磁盘快照实现跨进程撤销。

### 5.4 外部修改检测（TUI）

每 4 秒比较 `events.json` 的 `st_mtime_ns`；变化且当前没有弹窗时重新加载并刷新，避免两个终端实例互相覆盖。自身写盘后立即同步 mtime，防止误判。

---

## 6. 容错与校验

| 层 | 措施 |
| --- | --- |
| 输入 | 标题非空、时间格式、提前分钟非负、位置 ≥1、标签清洗、路径存在 |
| 模型 | `from_dict` 兼容缺字段；非法附件跳过；`done` 与 `done_at` 自动同步 |
| 存储 | 原子写 + fsync；`.bak` 回退；损坏文件隔离；备份轮转；路径穿越防护 |
| 业务 | 撤销/重做；删除确认；附件延迟清理；导入去重；未知字段拒绝 |
| UI | 错误 Toast/通知；保存失败阻止关闭；外部修改提示；全屏 Form 支持 Esc 取消 |

---

## 7. UI 设计

### 7.1 TUI（Textual）

- 深色主题，44 列侧边栏 + 详情区；
- 任务卡片两行：`◻ 标题 Pn` / `截止 · 标签 · 附件数`；
- 优先级颜色：逾期红、今天黄、未来灰、完成灰；
- 鼠标：点左侧方框勾选，单击选中，双击编辑；
- 模态：新增/编辑、确认、附件、路径、帮助；
- 底部 Footer 展示高频快捷键，`?` 打开完整表格。

### 7.2 GUI（PySide6）

- Material 风格：大圆角卡片、左侧 4px 色条、柔和阴影（用边框代替，避免性能问题）、主色 `#4f6ef7`；
- 三栏：252px 侧栏 / 420px 列表 / 自适应编辑器；
- 统计卡、完成度进度条、标签 chips、P1–P6 颜色循环；
- 浅色/深色两套 QSS，主题写入配置；
- 附件坞使用 `QListWidget` IconMode，支持拖入、内部排序、双击打开、右键移除；
- Toast 使用透明度动画，显示在右下角；
- 系统托盘提供显示/新建/退出与提醒气泡；
- 所有编辑字段防抖自动保存，避免“忘点保存”。

---

## 8. 测试设计

| 层 | 方式 | 数量 |
| --- | --- | --- |
| 核心 | `unittest`，临时数据目录 | 23 |
| CLI | 捕获 stdout，跨进程撤销 | 5 |
| TUI | Textual `run_test` + `Pilot` 键盘/鼠标 | 6 |
| GUI | Qt offscreen + 真实控件调用 | 6 |
| C 版 | 管道输入 + 文件内容比对 | 手工 |

重点用例：

- 插入顺延（题目原例）；
- 主文件损坏后从 `.bak` 恢复并告警；
- 路径穿越被拒绝；
- `.mjnote` 带附件往返；
- todo.txt 完成行与优先级；
- TUI 鼠标勾选、双击编辑；
- GUI 剪贴板图片粘贴；
- CLI `done → undo → redo`。

---

## 9. 取舍说明

1. **为什么 JSON 也算“文本文件”**：题目要求文本存储，JSON 是纯 UTF-8 文本，且可直接打开、diff、修复；附件二进制单独存放，避免把文件变成不可读的巨型 base64。
2. **为什么正文不内联图片**：保证笔记是可移植的 Markdown；媒体作为独立附件管理，才能在附件坞里自由拖动排序。代价是正文里没有缩略图，这是明确的取舍。
3. **为什么 Python + C 双实现**：C 版逐条覆盖基础要求，适合按 VC++6.0 题意验收；Python 版完成 pip 安装、双 UI、附件等拓展。两者共享同样的优先级规则。
4. **为什么附件删除不立即删文件**：为了让 `Ctrl+Z` 真正能恢复；孤儿文件通过 `gc_orphans()` 回收。

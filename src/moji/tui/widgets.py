"""TUI 自定义控件与弹窗。"""

from __future__ import annotations

import unicodedata
from datetime import timedelta
from pathlib import Path

from rich.text import Text
from textual.app import ComposeResult
from textual.binding import Binding
from textual.containers import Horizontal, Vertical, VerticalScroll
from textual.message import Message
from textual.screen import ModalScreen
from textual.widgets import (
    Button,
    Checkbox,
    Input,
    Markdown,
    OptionList,
    Static,
    TextArea,
)
from textual.widgets.option_list import Option

from ..core.models import Event, humanize_due, now, parse_datetime
from ..core.storage import human_size

ACCENT = "#5b8def"


def display_width(text: str) -> int:
    return sum(2 if unicodedata.east_asian_width(ch) in ("W", "F") else 1 for ch in text)


def truncate(text: str, limit: int = 28) -> str:
    if display_width(text) <= limit:
        return text
    out, used = [], 0
    for ch in text:
        cw = 2 if unicodedata.east_asian_width(ch) in ("W", "F") else 1
        if used + cw > limit - 1:
            break
        out.append(ch)
        used += cw
    return "".join(out) + "…"


def due_style(event: Event) -> str:
    if event.done:
        return "#56607a"
    if event.is_overdue:
        return "#f87171"
    if event.is_due_today():
        return "#fbbf24"
    return "#8b93a7"


class EventRow(Static):
    """侧边栏里的一行：两行展示标题、优先级、截止与标签。"""

    class Toggle(Message):
        def __init__(self, event_id: str) -> None:
            self.event_id = event_id
            super().__init__()

    class DoubleClicked(Message):
        def __init__(self, event_id: str) -> None:
            self.event_id = event_id
            super().__init__()

    def __init__(self, event: Event) -> None:
        super().__init__()
        self.event = event

    def render(self) -> Text:
        e = self.event
        text = Text()
        mark = "✔" if e.done else "◻"
        text.append(f"{mark} ", style="#4ade80" if e.done else "#6b7488")
        title_style = "#56607a strike" if e.done else "bold #e6e9ef"
        text.append(truncate(e.title, 27), style=title_style)
        text.append(f"  P{e.priority}", style="#5b8def" if not e.done else "#3d4a66")
        text.append("\n   ")
        if e.done:
            text.append("已完成", style="#56607a")
        else:
            text.append(humanize_due(e.due), style=due_style(e))
        if e.tags:
            text.append("  " + " ".join("#" + t for t in e.tags[:3]), style="#7c5cff")
        if e.attachments:
            text.append(f"  ⧉{len(e.attachments)}", style="#26a69a")
        return text

    def on_click(self, event) -> None:
        # 点击最左侧的方框 = 勾选/取消，不必先选中
        if event.x <= 2:
            event.stop()
            if getattr(event, "chain", 1) < 2:
                self.post_message(self.Toggle(self.event.id))
        elif getattr(event, "chain", 1) >= 2:
            event.stop()
            self.post_message(self.DoubleClicked(self.event.id))


class ConfirmScreen(ModalScreen[bool]):
    BINDINGS = [Binding("escape", "cancel", "取消")]

    def __init__(self, message: str, confirm_text: str = "确认删除") -> None:
        super().__init__()
        self.message = message
        self.confirm_text = confirm_text

    def compose(self) -> ComposeResult:
        with Vertical(classes="dialog small"):
            yield Static("请确认", classes="dialog-title")
            yield Static(self.message, classes="dialog-text")
            with Horizontal(classes="buttons"):
                yield Button("取消", id="cancel")
                yield Button(self.confirm_text, id="ok", variant="error")

    def on_mount(self) -> None:
        self.query_one("#ok", Button).focus()

    def on_button_pressed(self, event: Button.Pressed) -> None:
        self.dismiss(event.button.id == "ok")

    def action_cancel(self) -> None:
        self.dismiss(False)


class EventFormScreen(ModalScreen[dict]):
    """新增 / 编辑事件的表单（回车保存，Esc 取消）。"""

    BINDINGS = [
        Binding("escape", "cancel", "取消"),
        Binding("ctrl+s", "save", "保存"),
    ]

    def __init__(self, event: Event | None = None) -> None:
        super().__init__()
        self.event = event

    def compose(self) -> ComposeResult:
        e = self.event
        title = "编辑记事" if e else "新建记事"
        with Vertical(classes="dialog"):
            yield Static(("✎ " if e else "＋ ") + title, classes="dialog-title")
            yield Input(value=e.title if e else "", placeholder="标题：要做什么？", id="f-title")
            with Horizontal(classes="row"):
                yield Input(
                    value=f"{e.due:%Y-%m-%d %H:%M}" if e and e.due else "",
                    placeholder="截止：2026-11-24 18:00（留空=无期限）",
                    id="f-due",
                )
                yield Input(
                    value=str(e.remind_before) if e else "0",
                    placeholder="提前提醒(分钟)",
                    id="f-remind",
                    classes="w-sm",
                )
                yield Input(
                    value=str(e.priority) if e else "",
                    placeholder="优先级位置",
                    id="f-pos",
                    classes="w-sm",
                )
            with Horizontal(classes="quick-row"):
                yield Button("今天 18:00", id="q-today", classes="quick")
                yield Button("明天 09:00", id="q-tomorrow", classes="quick")
                yield Button("下周一 09:00", id="q-nextweek", classes="quick")
                yield Button("清除时间", id="q-clear", classes="quick")
            yield Input(
                value=", ".join(e.tags) if e else "",
                placeholder="标签：课程, 截止（逗号分隔，可留空）",
                id="f-tags",
            )
            yield TextArea(
                e.note if e else "",
                id="f-note",
                placeholder="备注（支持 Markdown，⌃S 保存）",
            )
            with Horizontal(classes="buttons"):
                yield Button("取消 (Esc)", id="cancel")
                yield Button("保存 (⌃S)", id="save", variant="primary")

    def on_mount(self) -> None:
        self.query_one("#f-title", Input).focus()

    def on_input_submitted(self, event: Input.Submitted) -> None:
        self._save()

    # 快捷时间按钮
    def on_button_pressed(self, event: Button.Pressed) -> None:
        bid = event.button.id
        due_input = self.query_one("#f-due", Input)
        if bid == "cancel":
            self.dismiss(None)
        elif bid == "save":
            self._save()
        elif bid == "q-today":
            due_input.value = now().replace(hour=18, minute=0, second=0, microsecond=0).strftime("%Y-%m-%d %H:%M")
        elif bid == "q-tomorrow":
            due_input.value = (now() + timedelta(days=1)).replace(
                hour=9, minute=0, second=0, microsecond=0
            ).strftime("%Y-%m-%d %H:%M")
        elif bid == "q-nextweek":
            days = 7 - now().weekday()
            due_input.value = (now() + timedelta(days=days)).replace(
                hour=9, minute=0, second=0, microsecond=0
            ).strftime("%Y-%m-%d %H:%M")
        elif bid == "q-clear":
            due_input.value = ""

    def _save(self) -> None:
        title = self.query_one("#f-title", Input).value.strip()
        if not title:
            self.app.notify("标题不能为空", severity="error")
            self.query_one("#f-title", Input).focus()
            return
        due_text = self.query_one("#f-due", Input).value.strip()
        due = None
        if due_text:
            due = parse_datetime(due_text)
            if due is None:
                self.app.notify(f"看不懂这个时间：{due_text}", severity="error", markup=False)
                return
        tags = [
            t.strip().lstrip("#")
            for t in self.query_one("#f-tags", Input).value.replace("，", ",").split(",")
            if t.strip()
        ]
        try:
            remind = int(self.query_one("#f-remind", Input).value.strip() or 0)
            if remind < 0:
                raise ValueError
        except ValueError:
            self.app.notify("提前提醒需要是非负整数（分钟）", severity="error")
            return
        pos_text = self.query_one("#f-pos", Input).value.strip()
        pos = None
        if pos_text:
            try:
                pos = int(pos_text)
                if pos < 1:
                    raise ValueError
            except ValueError:
                self.app.notify("优先级位置需要是 ≥1 的整数", severity="error")
                return
        note = self.query_one("#f-note", TextArea).text
        self.dismiss(
            {
                "title": title,
                "note": note,
                "due": due,
                "tags": tags,
                "remind_before": remind,
                "position": pos,
            }
        )

    def action_save(self) -> None:
        self._save()

    def action_cancel(self) -> None:
        self.dismiss(None)


class AttachmentScreen(ModalScreen[tuple]):
    """附件列表：回车打开，删除键移除。"""

    BINDINGS = [
        Binding("escape", "close", "关闭"),
        Binding("delete", "remove", "移除"),
        Binding("enter", "open", "打开"),
    ]

    def __init__(self, event: Event, repo) -> None:
        super().__init__()
        self.event = event
        self.repo = repo

    def compose(self) -> ComposeResult:
        with Vertical(classes="dialog small"):
            yield Static(f"⧉ 附件 · {self.event.title}", classes="dialog-title")
            if self.event.attachments:
                opts = []
                for i, att in enumerate(self.event.attachments):
                    exists = "" if self.repo.store.attachment_exists(att) else "（文件丢失）"
                    opts.append(
                        Option(
                            f"{att.name}   {human_size(att.size)} · {att.mime}{exists}",
                            id=str(i),
                        )
                    )
                yield OptionList(*opts, id="att-list", markup=False)
            else:
                yield Static("这个事件还没有附件。\n在 GUI 里可以直接拖入图片、音频或任意文件。", classes="dialog-text")
            with Horizontal(classes="buttons"):
                yield Button("关闭", id="close")
                if self.event.attachments:
                    yield Button("移除", id="remove", variant="error")
                    yield Button("打开", id="open", variant="primary")

    def on_mount(self) -> None:
        try:
            self.query_one("#att-list", OptionList).focus()
        except Exception:
            pass

    def _index(self) -> int:
        try:
            option = self.query_one("#att-list", OptionList).highlighted
            return int(option) if option is not None else -1
        except Exception:
            return -1

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "close":
            self.dismiss(None)
        elif event.button.id == "remove":
            self.action_remove()
        elif event.button.id == "open":
            self.action_open()

    def action_open(self) -> None:
        idx = self._index()
        if idx >= 0:
            self.dismiss(("open", idx))

    def action_remove(self) -> None:
        idx = self._index()
        if idx >= 0:
            self.dismiss(("remove", idx))

    def action_close(self) -> None:
        self.dismiss(None)


class PathScreen(ModalScreen[dict]):
    """导入/导出路径输入。"""

    BINDINGS = [Binding("escape", "cancel", "取消")]

    def __init__(self, mode: str, default_path: Path) -> None:
        super().__init__()
        self.mode = mode
        self.default_path = default_path

    def compose(self) -> ComposeResult:
        verb = "导出到" if self.mode == "export" else "从文件导入"
        with Vertical(classes="dialog small"):
            yield Static(("⬇ " if self.mode == "export" else "⬆ ") + verb, classes="dialog-title")
            yield Input(value=str(self.default_path), id="p-path")
            yield Static(
                "导出：.mjnote 含附件（跨设备迁移） / .json 纯数据 / .txt todo.txt / .md 可打印",
                classes="dialog-hint",
            )
            if self.mode == "import":
                yield Checkbox("清空现有数据后再导入（危险）", id="p-replace")
            with Horizontal(classes="buttons"):
                yield Button("取消", id="cancel")
                yield Button("确定", id="ok", variant="primary")

    def on_mount(self) -> None:
        self.query_one("#p-path", Input).focus()

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "cancel":
            self.dismiss(None)
        elif event.button.id == "ok":
            self._ok()

    def on_input_submitted(self, event: Input.Submitted) -> None:
        self._ok()

    def _ok(self) -> None:
        path = self.query_one("#p-path", Input).value.strip()
        if not path:
            self.app.notify("请填写文件路径", severity="error")
            return
        result = {"path": Path(path).expanduser()}
        if self.mode == "import":
            try:
                result["merge"] = not self.query_one("#p-replace", Checkbox).value
            except Exception:
                result["merge"] = True
        self.dismiss(result)

    def action_cancel(self) -> None:
        self.dismiss(None)


HELP_MD = """\
# 墨记 · 快捷键

## 浏览
| 按键 | 作用 |
|---|---|
| `↑` `↓` / `k` `j` | 上下选择 |
| `Enter` / `e` | 编辑选中事件 |
| `Space` / `d` | 勾选 / 取消勾选 |
| `Delete` / `x` | 删除（会先确认） |
| `[` / `]` | 提高 / 降低优先级（链表重排） |
| `o` | 查看附件 |

## 过滤与搜索
| 按键 | 作用 |
|---|---|
| `/` | 聚焦搜索框 |
| `t` | 今日安排 |
| `0` | 全部 |
| `1` | 未完成 |
| `2` | 已完成 |
| `3` | 已逾期 |
| `Esc` | 清空搜索/退出输入 |

## 数据
| 按键 | 作用 |
|---|---|
| `a` | 新增事件 |
| `Ctrl+Z` / `Ctrl+Y` | 撤销 / 重做 |
| `E` | 导出 |
| `I` | 导入 |
| `r` | 刷新 |
| `?` | 本帮助 |
| `q` | 退出 |

> 优先级规则：在第 N 位插入新事件，原第 N 位及之后的事件优先级自动 +1。
> 数据保存在 `~/.moji/events.json`（可用 `MOJI_HOME` 或 `--data-dir` 改）。
"""


class HelpScreen(ModalScreen[None]):
    BINDINGS = [Binding("escape", "close", "关闭"), Binding("question_mark", "close", "关闭")]

    def compose(self) -> ComposeResult:
        with Vertical(classes="dialog tall"):
            yield Static("? 使用帮助", classes="dialog-title")
            with VerticalScroll(id="help-scroll"):
                yield Markdown(HELP_MD)
            with Horizontal(classes="buttons"):
                yield Button("关闭 (Esc)", id="close", variant="primary")

    def on_button_pressed(self, event: Button.Pressed) -> None:
        self.dismiss(None)

    def action_close(self) -> None:
        self.dismiss(None)

"""墨记 TUI —— 用 Textual 构建的终端界面。

布局：左侧任务栏（可鼠标点击勾选）+ 右侧详情 / 备注预览，
底部快捷键，30 秒检查一次提醒（响铃 + 通知）。
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

from rich.markup import escape

from textual.app import App, ComposeResult
from textual.binding import Binding
from textual.containers import Horizontal, Vertical, VerticalScroll
from textual.css.query import NoMatches
from textual.screen import ModalScreen
from textual.widgets import Footer, Header, Input, ListView, ListItem, Markdown, Static

from ..core import importer, reminders
from ..core.models import Event, humanize_due, now
from ..core.repository import RepoError, Repository
from .widgets import (
    AttachmentScreen,
    ConfirmScreen,
    EventFormScreen,
    EventRow,
    HelpScreen,
    PathScreen,
    due_style,
)

FILTERS = {
    "all": "全部",
    "today": "今日",
    "pending": "未完成",
    "done": "已完成",
    "overdue": "已逾期",
}


class MojiTUI(App):
    TITLE = "墨记 MoJi"
    SUB_TITLE = "电子记事本 · TUI"

    BINDINGS = [
        Binding("a", "add", "新增", show=True),
        Binding("e", "edit", "编辑", show=True),
        Binding("space", "toggle_done", "勾选", show=True),
        Binding("d", "toggle_done", "勾选", show=False),
        Binding("x", "delete", "删除", show=False),
        Binding("delete", "delete", "删除", show=False),
        Binding("t", "filter('today')", "今日", show=True),
        Binding("0", "filter('all')", "全部", show=False),
        Binding("1", "filter('pending')", "未完成", show=False),
        Binding("2", "filter('done')", "已完成", show=False),
        Binding("3", "filter('overdue')", "逾期", show=False),
        Binding("slash", "search", "搜索", show=True),
        Binding("escape", "clear_search", "清除搜索", show=False),
        Binding("o", "attachments", "附件", show=False),
        Binding("left_square_bracket", "prior_up", "提优先级", show=False),
        Binding("right_square_bracket", "prior_down", "降优先级", show=False),
        Binding("ctrl+z", "undo", "撤销", show=False),
        Binding("ctrl+y", "redo", "重做", show=False),
        Binding("E", "export", "导出", show=True),
        Binding("I", "import_", "导入", show=False),
        Binding("r", "refresh", "刷新", show=False),
        Binding("question_mark", "help", "帮助", show=True),
        Binding("q", "quit", "退出", show=True),
    ]

    CSS = """
    Screen {
        background: #0b0e14;
        color: #e6e9ef;
    }
    Header {
        background: #0f1320;
        color: #e6e9ef;
    }
    Footer {
        background: #0f1320;
    }
    Footer > .footer--key {
        background: #1b2438;
        color: #aab3c5;
    }
    #body { height: 1fr; }
    #sidebar {
        width: 44;
        background: #0d111c;
        border-right: solid #1d2433;
        padding: 0 1;
    }
    #stats {
        height: auto;
        padding: 1 1;
        color: #8b93a7;
    }
    #search {
        height: 3;
        margin: 1 0;
        background: #121722;
        border: tall #232b3e;
    }
    #search:focus { border: tall #5b8def; }
    #list {
        height: 1fr;
        background: transparent;
        scrollbar-size-vertical: 1;
    }
    ListView > ListItem {
        height: auto;
        padding: 0;
        background: transparent;
    }
    ListView > ListItem.--highlight { background: #1b2438; }
    ListView:focus > ListItem.--highlight { background: #223052; }
    EventRow {
        height: 2;
        padding: 0 1;
        text-overflow: ellipsis;
    }
    EventRow:hover { background: #151b28; }
    .empty-row {
        height: auto;
        padding: 1;
        color: #56607a;
    }
    #hint {
        height: auto;
        padding: 1 1;
        color: #4c566e;
    }
    #detail {
        padding: 1 2;
        scrollbar-size-vertical: 1;
    }
    #detail-title {
        height: auto;
        text-style: bold;
        color: #ffffff;
    }
    #detail-meta {
        height: auto;
        margin-bottom: 1;
        color: #8b93a7;
    }
    #detail-note { background: transparent; }
    #detail-attachments {
        height: auto;
        margin-top: 1;
        color: #26a69a;
    }

    /* ---------------- 弹窗 ---------------- */
    EventFormScreen, ConfirmScreen, AttachmentScreen, PathScreen, HelpScreen {
        align: center middle;
        background: #060910 80%;
    }
    .dialog {
        width: 94;
        height: auto;
        max-height: 95%;
        background: #121722;
        border: round #2a3550;
        padding: 1 2;
    }
    .dialog.small { width: 80; }
    .dialog.tall { height: 90%; }
    .dialog-title {
        height: auto;
        padding-bottom: 1;
        text-style: bold;
        color: #ffffff;
    }
    .dialog-text { height: auto; padding: 1 0; color: #c3c9d6; }
    .dialog-hint { height: auto; color: #6b7488; }
    .dialog Input {
        background: #0f1320;
        border: tall #232b3e;
        margin-bottom: 1;
    }
    .dialog Input:focus { border: tall #5b8def; }
    .dialog TextArea {
        height: 8;
        background: #0f1320;
        border: tall #232b3e;
    }
    .row { height: 3; }
    .w-sm { width: 18; }
    .quick-row { height: 2; }
    .quick {
        height: 1;
        min-width: 9;
        border: none;
        background: #1b2438;
        color: #aab3c5;
        padding: 0 1;
        margin-right: 1;
    }
    .quick:hover { background: #223052; }
    .buttons {
        height: 3;
        padding-top: 1;
        align: right middle;
    }
    .buttons Button { margin-left: 1; }
    #help-scroll { height: 1fr; }
    #att-list {
        height: auto;
        max-height: 16;
        background: #0f1320;
        border: tall #232b3e;
    }
    """

    def __init__(self, repo: Repository) -> None:
        super().__init__()
        self.repo = repo
        self.filter_mode = "all"
        self.search_text = ""
        self.selected_id: str | None = None
        self._mtime_ns = 0
        self._search_timer = None

    # ------------------------------------------------------------ 生命周期
    def compose(self) -> ComposeResult:
        yield Header(show_clock=True)
        with Horizontal(id="body"):
            with Vertical(id="sidebar"):
                yield Static("", id="stats")
                yield Input(placeholder="🔍 搜索标题、标签、正文…", id="search")
                yield ListView(id="list")
                yield Static("↑↓ 选择　Space 勾选　a 新增　? 帮助", id="hint")
            with VerticalScroll(id="detail"):
                yield Static("", id="detail-title")
                yield Static("", id="detail-meta")
                yield Markdown("", id="detail-note")
                yield Static("", id="detail-attachments")
        yield Footer()

    async def on_mount(self) -> None:
        await self.refresh_list()
        self._sync_mtime()
        self.query_one("#list", ListView).focus()
        self.set_interval(30, self._tick)
        self.set_interval(4, self._check_external_change)
        for msg in self.repo.load_warnings:
            self.notify(msg, title="数据提示", severity="warning", timeout=8)
        if self.repo.today():
            self.bell()
        self.notify(
            reminders.startup_summary(self.repo),
            title="墨记已就绪",
            timeout=6,
            markup=False,
        )
        self._check_reminders()

    def on_unmount(self) -> None:
        # 退出时取消所有后台刷新任务，避免界面销毁后仍访问控件
        self.workers.cancel_all()

    def _sync_mtime(self) -> None:
        try:
            self._mtime_ns = self.repo.store.events_path.stat().st_mtime_ns
        except OSError:
            self._mtime_ns = 0

    # ------------------------------------------------------------ 数据刷新
    def visible_events(self) -> list[Event]:
        if self.filter_mode == "today":
            base = self.repo.today()
        elif self.filter_mode == "pending":
            base = self.repo.pending()
        elif self.filter_mode == "done":
            base = self.repo.finished()
        elif self.filter_mode == "overdue":
            base = self.repo.overdue()
        else:
            base = self.repo.all()
        text = self.search_text.strip().lower()
        if not text:
            return base
        terms = text.split()
        result = []
        for event in base:
            haystack = " ".join(
                [event.title, event.note, " ".join(event.tags)]
                + [a.name for a in event.attachments]
            ).lower()
            if all(term in haystack for term in terms):
                result.append(event)
        return result

    async def refresh_list(self, keep_id: str | None = None) -> None:
        keep_id = keep_id or self.selected_id
        try:
            lv = self.query_one("#list", ListView)
        except NoMatches:
            return
        await lv.clear()
        events = self.visible_events()
        for event in events:
            await lv.append(ListItem(EventRow(event), id=f"row-{event.id}"))
        if not events:
            await lv.append(ListItem(Static("这里空空如也，按 a 新建一条吧", classes="empty-row")))
        index = 0
        if events:
            for i, event in enumerate(events):
                if event.id == keep_id:
                    index = i
                    break
            if index < len(events):
                self.selected_id = events[index].id
            lv.index = index
        else:
            self.selected_id = None
        await self.update_detail()
        self.update_stats()
        self._sync_mtime()

    async def update_detail(self) -> None:
        try:
            title_widget = self.query_one("#detail-title", Static)
            meta_widget = self.query_one("#detail-meta", Static)
            note_widget = self.query_one("#detail-note", Markdown)
            att_widget = self.query_one("#detail-attachments", Static)
        except NoMatches:
            return
        event = self.repo.get(self.selected_id) if self.selected_id else None
        if event is None:
            title_widget.update("[#56607a]未选择记事[/]")
            meta_widget.update("")
            await note_widget.update("按 **a** 新建，或从左侧选择一条记事。")
            att_widget.update("")
            return
        mark = "✔" if event.done else "○"
        color = "#4ade80" if event.done else due_style(event)
        title_widget.update(f"[{color}]{mark}[/] {escape(event.title)}")
        bits = [f"P{event.priority}", escape(humanize_due(event.due))]
        if event.tags:
            bits.append(escape(" ".join("#" + t for t in event.tags)))
        if event.attachments:
            bits.append(f"⧉ {len(event.attachments)} 个附件（o 查看）")
        if event.remind_before:
            bits.append(f"提前 {event.remind_before} 分钟提醒")
        meta_widget.update("  ·  ".join(bits))
        await note_widget.update(event.note.strip() or "*暂无备注，按 e 编辑*")
        if event.attachments:
            lines = []
            for att in event.attachments:
                ok = "" if self.repo.store.attachment_exists(att) else "  [#f87171]文件丢失[/]"
                lines.append(f"⧉ {escape(att.name)}{ok}")
            att_widget.update("\n".join(lines))
        else:
            att_widget.update("")

    def update_stats(self) -> None:
        stats = self.repo.stats()
        label = FILTERS.get(self.filter_mode, "全部")
        text = (
            f"[#e6e9ef]今日[/] {stats['today']}　[#f87171]逾期[/] {stats['overdue']}\n"
            f"[#8b93a7]待办[/] {stats['pending']}　[#4ade80]完成[/] {stats['rate']}\n"
            f"[#5b8def]◈ {label}[/]"
        )
        if self.search_text:
            text += f"　[#7c5cff]“{escape(self.search_text)}”[/]"
        try:
            self.query_one("#stats", Static).update(text)
        except NoMatches:
            pass

    # ------------------------------------------------------------ 事件
    async def on_list_view_highlighted(self, event: ListView.Highlighted) -> None:
        if event.item is None:
            return
        try:
            row = event.item.query_one(EventRow)
        except Exception:
            return
        self.selected_id = row.event.id
        await self.update_detail()

    async def on_event_row_toggle(self, message: EventRow.Toggle) -> None:
        await self._toggle(message.event_id)

    def on_event_row_double_clicked(self, message: EventRow.DoubleClicked) -> None:
        self.selected_id = message.event_id
        self.action_edit()

    def on_input_changed(self, event: Input.Changed) -> None:
        if event.input.id == "search":
            self.search_text = event.value
            if self._search_timer is not None:
                self._search_timer.stop()
            self._search_timer = self.set_timer(
                0.15,
                lambda: self.run_worker(
                    self.refresh_list(keep_id=self.selected_id), exclusive=True
                ),
            )

    # ------------------------------------------------------------ 增删改
    async def _apply_form(self, result: dict, event: Event | None) -> None:
        try:
            if event is None:
                created = self.repo.add(
                    result["title"],
                    note=result["note"],
                    due=result["due"],
                    tags=result["tags"],
                    position=result["position"],
                    remind_before=result["remind_before"],
                )
                self.selected_id = created.id
                self.notify(f"已加入 P{created.priority}：{created.title}", timeout=3, markup=False)
            else:
                self.repo.update(
                    event.id,
                    title=result["title"],
                    note=result["note"],
                    due=result["due"],
                    tags=result["tags"],
                    remind_before=result["remind_before"],
                )
                if result["position"] and result["position"] != event.priority:
                    self.repo.move_to(event.id, result["position"])
                self.selected_id = event.id
                self.notify("已保存修改", timeout=3)
        except RepoError as exc:
            self.notify(str(exc), severity="error", markup=False)
        self._sync_mtime()
        await self.refresh_list(keep_id=self.selected_id)

    def _focus_list(self) -> None:
        try:
            self.query_one("#list", ListView).focus()
        except NoMatches:
            pass

    # 弹窗统一走「push_screen + 回调」，回调里再用 worker 跑异步逻辑。
    # 这样可以避开 push_screen_wait 必须在 worker 中调用的限制。
    def action_add(self) -> None:
        self.push_screen(EventFormScreen(), callback=self._on_add_result)

    def _on_add_result(self, result) -> None:
        self._focus_list()
        if result:
            self.run_worker(self._apply_form(result, None), exclusive=True)

    def action_edit(self) -> None:
        event = self.repo.get(self.selected_id) if self.selected_id else None
        if event is None:
            self.notify("先选中一条记事", severity="warning")
            return

        def done(result) -> None:
            self._focus_list()
            if result:
                self.run_worker(self._apply_form(result, event), exclusive=True)

        self.push_screen(EventFormScreen(event), callback=done)

    async def _toggle(self, event_id: str) -> None:
        try:
            event = self.repo.toggle_done(event_id)
        except RepoError as exc:
            self.notify(str(exc), severity="error", markup=False)
            return
        if event.done:
            self.notify(f"完成：{event.title}", timeout=2, markup=False)
        self._sync_mtime()
        await self.refresh_list(keep_id=event.id)

    async def action_toggle_done(self) -> None:
        if self.selected_id:
            await self._toggle(self.selected_id)

    def action_delete(self) -> None:
        event = self.repo.get(self.selected_id) if self.selected_id else None
        if event is None:
            return

        def done(ok) -> None:
            self._focus_list()
            if ok:
                self.run_worker(self._delete_now(event.id), exclusive=True)

        self.push_screen(
            ConfirmScreen(f"删除「{event.title}」？\n可用 Ctrl+Z 撤销。"), callback=done
        )

    async def _delete_now(self, event_id: str) -> None:
        try:
            self.repo.delete(event_id)
        except RepoError as exc:
            self.notify(str(exc), severity="error", markup=False)
            return
        self.selected_id = None
        self._sync_mtime()
        await self.refresh_list()
        self.notify("已删除（Ctrl+Z 可撤销）", timeout=3)

    async def action_prior_up(self) -> None:
        await self._move(-1)

    async def action_prior_down(self) -> None:
        await self._move(1)

    async def _move(self, delta: int) -> None:
        if not self.selected_id:
            return
        try:
            event = self.repo.move_by(self.selected_id, delta)
        except RepoError as exc:
            self.notify(str(exc), severity="error", markup=False)
            return
        self._sync_mtime()
        await self.refresh_list(keep_id=event.id)
        self.notify(f"{event.title} → P{event.priority}", timeout=2, markup=False)

    # ------------------------------------------------------------ 撤销 / 筛选
    async def action_undo(self) -> None:
        ok = self.repo.undo()
        self.notify("已撤销" if ok else "没有可撤销的操作", severity="information" if ok else "warning")
        self._sync_mtime()
        await self.refresh_list(keep_id=self.selected_id)

    async def action_redo(self) -> None:
        ok = self.repo.redo()
        self.notify("已重做" if ok else "没有可重做的操作", severity="information" if ok else "warning")
        self._sync_mtime()
        await self.refresh_list(keep_id=self.selected_id)

    async def action_filter(self, mode: str) -> None:
        self.filter_mode = mode
        await self.refresh_list()
        self.notify(f"筛选：{FILTERS.get(mode, mode)}", timeout=1.5)

    def action_search(self) -> None:
        self.query_one("#search", Input).focus()

    async def action_clear_search(self) -> None:
        box = self.query_one("#search", Input)
        if box.value:
            box.value = ""
            self.search_text = ""
            await self.refresh_list(keep_id=self.selected_id)
        if box.has_focus:
            self._focus_list()

    # ------------------------------------------------------------ 附件 / 导入导出
    def action_attachments(self) -> None:
        event = self.repo.get(self.selected_id) if self.selected_id else None
        if event is None:
            self.notify("先选中一条记事", severity="warning")
            return
        self.push_screen(
            AttachmentScreen(event, self.repo),
            callback=lambda result: self._on_attachment_result(event.id, result),
        )

    def _on_attachment_result(self, event_id: str, result) -> None:
        self._focus_list()
        if result:
            self.run_worker(self._apply_attachment(event_id, result), exclusive=True)

    async def _apply_attachment(self, event_id: str, result: tuple) -> None:
        action, index = result
        event = self.repo.get(event_id)
        if not event or not 0 <= index < len(event.attachments):
            return
        att = event.attachments[index]
        if action == "open":
            try:
                path = self.repo.attachment_path(att)
            except Exception as exc:
                self.notify(str(exc), severity="error", markup=False)
                return
            if not path.is_file():
                self.notify("附件文件已丢失", severity="error")
                return
            try:
                _open_path(path)
                self.notify(f"正在打开 {att.name}", timeout=2, markup=False)
            except Exception as exc:
                self.notify(f"无法打开：{exc}", severity="error", markup=False)
        elif action == "remove":
            try:
                self.repo.remove_attachment(event_id, index)
            except RepoError as exc:
                self.notify(str(exc), severity="error", markup=False)
                return
            self._sync_mtime()
            self.notify("已移除附件（文件保留，Ctrl+Z 可恢复记录）", timeout=3)
            await self.refresh_list(keep_id=event_id)

    def action_export(self) -> None:
        default = Path.home() / f"moji-export-{now():%Y%m%d}.mjnote"
        self.push_screen(PathScreen("export", default), callback=self._on_export_path)

    def _on_export_path(self, result) -> None:
        self._focus_list()
        if result:
            self.run_worker(self._do_export(result["path"]), exclusive=True)

    async def _do_export(self, path: Path) -> None:
        try:
            out = importer.export_auto(self.repo, path)
        except RepoError as exc:
            self.notify(str(exc), severity="error", markup=False)
            return
        self.notify(f"已导出到 {out}", timeout=5, markup=False)

    def action_import_(self) -> None:
        default = Path.home() / "moji-import.json"
        self.push_screen(PathScreen("import", default), callback=self._on_import_path)

    def _on_import_path(self, result) -> None:
        self._focus_list()
        if result:
            self.run_worker(self._do_import(result), exclusive=True)

    async def _do_import(self, result: dict) -> None:
        try:
            report = importer.import_auto(
                self.repo, result["path"], merge=result.get("merge", True)
            )
        except RepoError as exc:
            self.notify(str(exc), severity="error", markup=False)
            return
        self._sync_mtime()
        await self.refresh_list()
        self.notify(
            f"导入完成：新增 {report['added']} 条，跳过 {report['skipped']} 条",
            timeout=5,
        )

    async def action_refresh(self) -> None:
        self.repo.reload()
        await self.refresh_list(keep_id=self.selected_id)
        self.notify("已刷新", timeout=2)

    def action_help(self) -> None:
        self.push_screen(HelpScreen(), callback=lambda _: self._focus_list())

    def action_quit(self) -> None:
        self.exit()

    # ------------------------------------------------------------ 定时任务
    async def _tick(self) -> None:
        self._check_external_change()
        self._check_reminders()
        self.update_stats()

    def _check_reminders(self) -> None:
        due = self.repo.due_reminders(now())
        if not due:
            return
        self.bell()
        self.notify(
            reminders.reminder_text(due),
            title="⏰ 待办提醒",
            severity="warning",
            timeout=15,
            markup=False,
        )
        self.repo.mark_reminded(due)
        self._sync_mtime()
        self.run_worker(self.refresh_list(keep_id=self.selected_id), exclusive=True)

    def _check_external_change(self) -> None:
        path = self.repo.store.events_path
        try:
            mtime = path.stat().st_mtime_ns
        except OSError:
            return
        if mtime == self._mtime_ns:
            return
        if isinstance(self.screen, ModalScreen):
            return  # 弹窗打开时先不刷新，避免打断输入
        self._mtime_ns = mtime
        self.repo.reload()
        self.run_worker(self.refresh_list(keep_id=self.selected_id))
        self.notify("检测到其他进程修改了数据，已自动刷新", timeout=3)


def _open_path(path: Path) -> None:
    if sys.platform.startswith("win"):
        os.startfile(str(path))  # type: ignore[attr-defined]
    elif sys.platform == "darwin":
        subprocess.Popen(["open", str(path)])
    else:
        subprocess.Popen(["xdg-open", str(path)])


def run(repo: Repository) -> None:
    MojiTUI(repo).run()

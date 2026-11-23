"""墨记 GUI 主窗口（Material Design 风格，PySide6）。

功能：
    - 左侧筛选 / 标签 / 统计，中间卡片流，右侧内联编辑
    - 截止时间 + 提前提醒，30 秒轮询并弹 Toast / 托盘通知
    - 附件拖拽、粘贴图片、拖动排序、双击打开、手绘笔迹
    - 导入/导出（.mjnote / .json / .txt / .md）、浅色/深色主题
"""

from __future__ import annotations

import os
import sys
import tempfile
from datetime import timedelta
from pathlib import Path

from PySide6.QtCore import QDateTime, Qt, QTimer, QUrl, Signal
from PySide6.QtGui import (
    QAction,
    QActionGroup,
    QColor,
    QDesktopServices,
    QFont,
    QIcon,
    QKeySequence,
    QPainter,
    QPixmap,
)
from PySide6.QtWidgets import (
    QApplication,
    QButtonGroup,
    QCheckBox,
    QDateTimeEdit,
    QDialog,
    QFileDialog,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidgetItem,
    QMainWindow,
    QMenu,
    QMessageBox,
    QProgressBar,
    QPushButton,
    QScrollArea,
    QSizePolicy,
    QSpinBox,
    QSplitter,
    QStackedWidget,
    QSystemTrayIcon,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

from .. import __app_name__, __version__
from ..core import importer, reminders
from ..core.models import Event, humanize_due, now
from ..core.repository import RepoError, Repository
from .theme import build_qss, palette
from .widgets import (
    AttachmentList,
    EventCard,
    SketchDialog,
    StatCard,
    Toast,
    build_attachment_items,
)


# ------------------------------------------------------------------ 控件
class NoteEdit(QTextEdit):
    """备注编辑器：粘贴图片或拖入文件时交给主窗口存成附件。"""

    files_dropped = Signal(list)
    image_pasted = Signal(object)

    def canInsertFromMimeData(self, source) -> bool:  # type: ignore[override]
        if source.hasImage() or source.hasUrls():
            return True
        return super().canInsertFromMimeData(source)

    def insertFromMimeData(self, source) -> None:  # type: ignore[override]
        if source.hasImage():
            image = source.imageData()
            if image is not None:
                self.image_pasted.emit(image)
                return
        if source.hasUrls():
            paths = [
                url.toLocalFile()
                for url in source.urls()
                if url.isLocalFile() and Path(url.toLocalFile()).is_file()
            ]
            if paths:
                self.files_dropped.emit(paths)
                return
        super().insertFromMimeData(source)

    def dragEnterEvent(self, event) -> None:  # type: ignore[no-untyped-def]
        if event.mimeData().hasUrls() or event.mimeData().hasImage():
            event.acceptProposedAction()
        else:
            super().dragEnterEvent(event)

    def dropEvent(self, event) -> None:  # type: ignore[no-untyped-def]
        if event.mimeData().hasUrls():
            paths = [
                url.toLocalFile()
                for url in event.mimeData().urls()
                if url.isLocalFile() and Path(url.toLocalFile()).is_file()
            ]
            if paths:
                self.files_dropped.emit(paths)
                event.acceptProposedAction()
                return
        super().dropEvent(event)


def make_app_icon(size: int = 256) -> QIcon:
    pix = QPixmap(size, size)
    pix.fill(Qt.transparent)
    painter = QPainter(pix)
    painter.setRenderHint(QPainter.Antialiasing)
    painter.setBrush(QColor("#4f6ef7"))
    painter.setPen(Qt.NoPen)
    radius = size * 0.24
    painter.drawRoundedRect(0, 0, size, size, radius, radius)
    painter.setPen(QColor("#ffffff"))
    font = QFont("Microsoft YaHei UI", int(size * 0.46))
    font.setBold(True)
    painter.setFont(font)
    painter.drawText(pix.rect(), Qt.AlignCenter, "墨")
    painter.end()
    return QIcon(pix)


# ------------------------------------------------------------------ 主窗口
class MainWindow(QMainWindow):
    FILTERS = [
        ("all", "全部"),
        ("today", "今天"),
        ("pending", "待办"),
        ("done", "已完成"),
        ("overdue", "逾期"),
    ]

    def __init__(self, repo: Repository) -> None:
        super().__init__()
        self.repo = repo
        cfg = repo.store.load_config()
        self.theme_name = str(cfg.get("theme") or "light")
        self.pal = palette(self.theme_name)
        self.filter_mode = "all"
        self.tag_filter: str | None = None
        self.search_text = ""
        self.current_id: str | None = None
        self._loading = False
        self._saving = False

        self.setWindowTitle(f"{__app_name__} · 电子记事本")
        self.setWindowIcon(make_app_icon())
        self.setMinimumSize(1020, 660)

        self._save_timer = QTimer(self)
        self._save_timer.setSingleShot(True)
        self._save_timer.setInterval(650)
        self._save_timer.timeout.connect(self.save_current)

        self._search_timer = QTimer(self)
        self._search_timer.setSingleShot(True)
        self._search_timer.setInterval(220)
        self._search_timer.timeout.connect(self._do_search_refresh)

        self._build_ui()
        self._build_menu()
        if QApplication.instance() is not None:
            QApplication.instance().setStyleSheet(build_qss(self.theme_name))
        self._restore_geometry(cfg)
        self.refresh_list(select_first=True)

        self.toast = Toast(self)

        self._remind_timer = QTimer(self)
        self._remind_timer.setInterval(30_000)
        self._remind_timer.timeout.connect(self.check_reminders)
        self._remind_timer.start()
        self._shot_mode = bool(os.environ.get("MOJI_SHOT"))
        if not self._shot_mode:
            QTimer.singleShot(900, self._greet_today)
            QTimer.singleShot(1800, self.check_reminders)
            self._setup_tray()
        else:
            self.tray = None

    # ---------------------------------------------------------- UI 搭建
    def _build_ui(self) -> None:
        root = QWidget()
        root.setObjectName("Root")
        layout = QHBoxLayout(root)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        layout.addWidget(self._build_sidebar())

        self.splitter = QSplitter(Qt.Horizontal)
        self.splitter.setChildrenCollapsible(False)
        self.splitter.setHandleWidth(2)
        self.splitter.addWidget(self._build_center())
        self.splitter.addWidget(self._build_editor_stack())
        self.splitter.setStretchFactor(0, 0)
        self.splitter.setStretchFactor(1, 1)
        self.splitter.setSizes([420, 680])
        layout.addWidget(self.splitter, 1)
        self.setCentralWidget(root)
        self.statusBar().showMessage(f"数据目录：{self.repo.store.dir}　·　本地自动保存")

    def _build_sidebar(self) -> QFrame:
        side = QFrame()
        side.setObjectName("Sidebar")
        side.setFixedWidth(252)
        layout = QVBoxLayout(side)
        layout.setContentsMargins(18, 20, 18, 16)
        layout.setSpacing(10)

        brand = QHBoxLayout()
        brand.setSpacing(12)
        mark = QFrame()
        mark.setObjectName("BrandMark")
        mark.setFixedSize(50, 50)
        mark_layout = QVBoxLayout(mark)
        mark_layout.setContentsMargins(0, 0, 0, 0)
        brand_text = QLabel("墨")
        brand_text.setObjectName("BrandMarkText")
        brand_text.setAlignment(Qt.AlignCenter)
        mark_layout.addWidget(brand_text)
        brand.addWidget(mark)
        titles = QVBoxLayout()
        titles.setSpacing(0)
        title = QLabel("墨记 MoJi")
        title.setProperty("role", "h1")
        subtitle = QLabel("电子记事本")
        subtitle.setProperty("role", "muted")
        titles.addWidget(title)
        titles.addWidget(subtitle)
        brand.addLayout(titles)
        brand.addStretch(1)
        layout.addLayout(brand)

        grid = QGridLayout()
        grid.setHorizontalSpacing(8)
        grid.setVerticalSpacing(8)
        self.stat_today = StatCard("今日安排")
        self.stat_pending = StatCard("待办")
        self.stat_overdue = StatCard("逾期")
        self.stat_rate = StatCard("完成率")
        grid.addWidget(self.stat_today, 0, 0)
        grid.addWidget(self.stat_pending, 0, 1)
        grid.addWidget(self.stat_overdue, 1, 0)
        grid.addWidget(self.stat_rate, 1, 1)
        layout.addLayout(grid)

        nav_label = QLabel("视图")
        nav_label.setProperty("role", "h3")
        layout.addWidget(nav_label)

        self.nav_group = QButtonGroup(self)
        self.nav_group.setExclusive(True)
        self.nav_buttons: dict[str, QPushButton] = {}
        self.nav_titles = {key: text for key, text in self.FILTERS}
        for key, text in self.FILTERS:
            button = QPushButton(text)
            button.setObjectName("NavButton")
            button.setCheckable(True)
            button.setCursor(Qt.PointingHandCursor)
            button.clicked.connect(lambda _checked=False, k=key: self.set_filter(k))
            self.nav_group.addButton(button)
            self.nav_buttons[key] = button
            layout.addWidget(button)
        self.nav_buttons["all"].setChecked(True)

        tags_header = QHBoxLayout()
        tags_title = QLabel("标签")
        tags_title.setProperty("role", "h3")
        tags_header.addWidget(tags_title)
        tags_header.addStretch(1)
        layout.addLayout(tags_header)
        self.tags_scroll = QScrollArea()
        self.tags_scroll.setWidgetResizable(True)
        self.tags_scroll.setMinimumHeight(88)
        self.tags_scroll.setMaximumHeight(230)
        self.tags_scroll.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Expanding)
        self.tags_scroll.setFrameShape(QFrame.NoFrame)
        self.tags_container = QWidget()
        self.tags_layout = QVBoxLayout(self.tags_container)
        self.tags_layout.setContentsMargins(0, 0, 0, 0)
        self.tags_layout.setSpacing(4)
        self.tags_layout.addStretch(1)
        self.tags_scroll.setWidget(self.tags_container)
        layout.addWidget(self.tags_scroll)

        progress_row = QHBoxLayout()
        progress_label = QLabel("完成度")
        progress_label.setProperty("role", "muted")
        self.progress = QProgressBar()
        self.progress.setRange(0, 100)
        self.progress.setValue(0)
        progress_row.addWidget(progress_label)
        progress_row.addWidget(self.progress, 1)
        layout.addLayout(progress_row)

        bottom = QHBoxLayout()
        self.theme_button = QPushButton("☀ 浅色" if self.theme_name == "dark" else "🌙 深色")
        self.theme_button.setObjectName("Ghost")
        self.theme_button.setCursor(Qt.PointingHandCursor)
        self.theme_button.clicked.connect(self.toggle_theme)
        data_button = QPushButton("📂 数据目录")
        data_button.setObjectName("Ghost")
        data_button.setCursor(Qt.PointingHandCursor)
        data_button.clicked.connect(self._open_data_dir)
        bottom.addWidget(self.theme_button)
        bottom.addWidget(data_button)
        bottom.addStretch(1)
        layout.addLayout(bottom)

        version = QLabel(f"v{__version__} · 数据保存在本地")
        version.setProperty("role", "muted")
        version.setStyleSheet("font-size: 9pt;")
        layout.addWidget(version)
        return side

    def _build_center(self) -> QFrame:
        panel = QFrame()
        panel.setObjectName("CenterPanel")
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(22, 18, 16, 12)
        layout.setSpacing(12)

        header = QHBoxLayout()
        title = QLabel("记事")
        title.setProperty("role", "h1")
        header.addWidget(title)
        header.addStretch(1)
        new_button = QPushButton("＋ 新建")
        new_button.setObjectName("Primary")
        new_button.setCursor(Qt.PointingHandCursor)
        new_button.setToolTip("新建记事 (Ctrl+N)")
        new_button.clicked.connect(self.new_event)
        header.addWidget(new_button)
        layout.addLayout(header)

        self.search_edit = QLineEdit()
        self.search_edit.setObjectName("SearchEdit")
        self.search_edit.setPlaceholderText("🔍  搜索标题、标签、正文…")
        self.search_edit.setClearButtonEnabled(True)
        self.search_edit.textChanged.connect(self._on_search_changed)
        layout.addWidget(self.search_edit)

        info_row = QHBoxLayout()
        self.count_label = QLabel("共 0 条")
        self.count_label.setProperty("role", "muted")
        info_row.addWidget(self.count_label)
        info_row.addStretch(1)
        self.filter_hint = QLabel("")
        self.filter_hint.setProperty("role", "muted")
        info_row.addWidget(self.filter_hint)
        layout.addLayout(info_row)

        self.cards_scroll = QScrollArea()
        self.cards_scroll.setWidgetResizable(True)
        self.cards_scroll.setFrameShape(QFrame.NoFrame)
        self.cards_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.cards_container = QWidget()
        self.cards_layout = QVBoxLayout(self.cards_container)
        self.cards_layout.setContentsMargins(0, 0, 6, 12)
        self.cards_layout.setSpacing(10)
        self.cards_layout.addStretch(1)
        self.cards_scroll.setWidget(self.cards_container)
        layout.addWidget(self.cards_scroll, 1)

        self.empty_list_label = QLabel("没有符合条件的记事\n点击右上角「＋ 新建」开始记录")
        self.empty_list_label.setAlignment(Qt.AlignCenter)
        self.empty_list_label.setProperty("role", "muted")
        self.empty_list_label.setMinimumHeight(180)
        self.empty_list_label.hide()
        layout.addWidget(self.empty_list_label)
        return panel

    def _build_editor_stack(self) -> QStackedWidget:
        self.editor_stack = QStackedWidget()
        self.editor_stack.setMinimumWidth(430)
        self.editor_stack.addWidget(self._build_editor())
        self.editor_stack.addWidget(self._build_empty_page())
        self.editor_stack.setCurrentIndex(1)
        return self.editor_stack

    def _build_editor(self) -> QFrame:
        panel = QFrame()
        panel.setObjectName("EditorPanel")
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(26, 20, 26, 14)
        layout.setSpacing(11)

        self.title_edit = QLineEdit()
        self.title_edit.setObjectName("TitleEdit")
        self.title_edit.setPlaceholderText("标题")
        self.title_edit.textEdited.connect(self._schedule_save)
        self.title_edit.editingFinished.connect(self.save_current)
        layout.addWidget(self.title_edit)

        self.meta_label = QLabel("")
        self.meta_label.setProperty("role", "muted")
        layout.addWidget(self.meta_label)

        due_row = QHBoxLayout()
        due_row.setSpacing(8)
        self.due_check = QCheckBox("截止")
        self.due_check.toggled.connect(self._on_due_toggled)
        self.due_edit = QDateTimeEdit()
        self.due_edit.setCalendarPopup(True)
        self.due_edit.setDisplayFormat("yyyy-MM-dd HH:mm")
        self.due_edit.setMinimumWidth(168)
        self.due_edit.dateTimeChanged.connect(self._schedule_save)
        self.remind_spin = QSpinBox()
        self.remind_spin.setRange(0, 10080)
        self.remind_spin.setSuffix(" 分钟前提醒")
        self.remind_spin.setToolTip("0 表示到点提醒；最多提前 7 天")
        self.remind_spin.valueChanged.connect(self._schedule_save)
        due_row.addWidget(self.due_check)
        due_row.addWidget(self.due_edit, 1)
        due_row.addWidget(self.remind_spin)
        layout.addLayout(due_row)

        self.tags_edit = QLineEdit()
        self.tags_edit.setPlaceholderText("标签：课程, 截止（逗号分隔）")
        self.tags_edit.textEdited.connect(self._schedule_save)
        self.tags_edit.editingFinished.connect(self.save_current)
        layout.addWidget(self.tags_edit)

        toolbar = QHBoxLayout()
        toolbar.setSpacing(6)
        self.save_button = QPushButton("保存")
        self.save_button.setObjectName("Primary")
        self.save_button.clicked.connect(lambda: self.save_current(show_toast=True))
        self.done_button = QPushButton("✓ 标记完成")
        self.done_button.setObjectName("Ghost")
        self.done_button.clicked.connect(self.toggle_current_done)
        duplicate = QPushButton("⧉ 复制")
        duplicate.setObjectName("Ghost")
        duplicate.setToolTip("复制这条记事")
        duplicate.clicked.connect(self.duplicate_current)
        up = QPushButton("↑")
        up.setObjectName("IconButton")
        up.setToolTip("提高优先级")
        up.clicked.connect(lambda: self.move_current(-1))
        down = QPushButton("↓")
        down.setObjectName("IconButton")
        down.setToolTip("降低优先级")
        down.clicked.connect(lambda: self.move_current(1))
        toolbar.addWidget(self.save_button)
        toolbar.addWidget(self.done_button)
        toolbar.addWidget(duplicate)
        toolbar.addWidget(up)
        toolbar.addWidget(down)
        toolbar.addStretch(1)
        undo = QPushButton("↶")
        undo.setObjectName("IconButton")
        undo.setToolTip("撤销应用操作 (Ctrl+Alt+Z)")
        undo.clicked.connect(self.undo_app)
        redo = QPushButton("↷")
        redo.setObjectName("IconButton")
        redo.setToolTip("重做 (Ctrl+Alt+Y)")
        redo.clicked.connect(self.redo_app)
        delete = QPushButton("删除")
        delete.setObjectName("Danger")
        delete.clicked.connect(self.delete_current)
        toolbar.addWidget(undo)
        toolbar.addWidget(redo)
        toolbar.addWidget(delete)
        layout.addLayout(toolbar)

        note_header = QHBoxLayout()
        note_title = QLabel("备注")
        note_title.setProperty("role", "h3")
        note_hint = QLabel("支持 Markdown · 可粘贴图片、拖入文件")
        note_hint.setProperty("role", "muted")
        note_hint.setStyleSheet("font-size: 9pt;")
        table_button = QPushButton("插入表格")
        table_button.setObjectName("Ghost")
        table_button.setStyleSheet("font-size: 9pt;")
        table_button.clicked.connect(self.insert_table)
        sketch_button = QPushButton("✎ 手绘")
        sketch_button.setObjectName("Ghost")
        sketch_button.setStyleSheet("font-size: 9pt;")
        sketch_button.setToolTip("用鼠标写写画画，保存为图片附件")
        sketch_button.clicked.connect(self.open_sketch)
        note_header.addWidget(note_title)
        note_header.addWidget(note_hint)
        note_header.addStretch(1)
        note_header.addWidget(table_button)
        note_header.addWidget(sketch_button)
        layout.addLayout(note_header)

        self.note_edit = NoteEdit()
        self.note_edit.setPlaceholderText("写点什么…")
        self.note_edit.setAcceptRichText(False)
        self.note_edit.setTabChangesFocus(True)
        self.note_edit.textChanged.connect(self._schedule_save)
        self.note_edit.files_dropped.connect(self.attach_files)
        self.note_edit.image_pasted.connect(self.attach_image)
        layout.addWidget(self.note_edit, 1)

        att_header = QHBoxLayout()
        att_title = QLabel("附件")
        att_title.setProperty("role", "h3")
        att_hint = QLabel("拖入文件 · 双击打开 · 拖动图标排序")
        att_hint.setProperty("role", "muted")
        att_hint.setStyleSheet("font-size: 9pt;")
        add_att = QPushButton("＋ 添加")
        add_att.setObjectName("Ghost")
        add_att.setStyleSheet("font-size: 9pt;")
        add_att.clicked.connect(self.choose_attachments)
        att_header.addWidget(att_title)
        att_header.addWidget(att_hint)
        att_header.addStretch(1)
        att_header.addWidget(add_att)
        layout.addLayout(att_header)

        self.att_list = AttachmentList()
        self.att_list.files_dropped.connect(self.attach_files)
        self.att_list.reordered.connect(self.reorder_attachments)
        self.att_list.remove_requested.connect(self.remove_attachment)
        self.att_list.itemDoubleClicked.connect(self.open_attachment)
        self.att_list.open_requested.connect(self.open_attachment_path)
        layout.addWidget(self.att_list)

        self.status_label = QLabel("")
        self.status_label.setProperty("role", "muted")
        self.status_label.setStyleSheet("font-size: 9pt;")
        layout.addWidget(self.status_label)
        return panel

    def _build_empty_page(self) -> QFrame:
        page = QFrame()
        page.setObjectName("EditorPanel")
        layout = QVBoxLayout(page)
        layout.setContentsMargins(40, 40, 40, 40)
        layout.addStretch(2)
        icon = QLabel("🗒")
        icon.setAlignment(Qt.AlignCenter)
        icon.setStyleSheet("font-size: 46pt;")
        title = QLabel("开始记录今天")
        title.setObjectName("EmptyTitle")
        title.setAlignment(Qt.AlignCenter)
        hint = QLabel("在左侧选择一条记事，或新建一条。\n所有内容都会自动保存到本地。")
        hint.setObjectName("EmptyHint")
        hint.setAlignment(Qt.AlignCenter)
        button = QPushButton("＋ 新建记事")
        button.setObjectName("Primary")
        button.setFixedWidth(160)
        button.setCursor(Qt.PointingHandCursor)
        button.clicked.connect(self.new_event)
        layout.addWidget(icon)
        layout.addSpacing(6)
        layout.addWidget(title)
        layout.addWidget(hint)
        layout.addSpacing(14)
        row = QHBoxLayout()
        row.addStretch(1)
        row.addWidget(button)
        row.addStretch(1)
        layout.addLayout(row)
        layout.addStretch(3)
        return page

    # ---------------------------------------------------------- 菜单
    def _build_menu(self) -> None:
        bar = self.menuBar()

        file_menu = bar.addMenu("文件")
        self._action(file_menu, "新建记事", "Ctrl+N", self.new_event)
        self._action(file_menu, "导入…", "Ctrl+O", self.import_data)
        self._action(file_menu, "导出…", "Ctrl+Shift+S", self.export_data)
        file_menu.addSeparator()
        self._action(file_menu, "退出", "Ctrl+Q", self.close)

        edit_menu = bar.addMenu("编辑")
        self._action(edit_menu, "撤销（应用操作）", "Ctrl+Alt+Z", self.undo_app)
        self._action(edit_menu, "重做", "Ctrl+Alt+Y", self.redo_app)
        edit_menu.addSeparator()
        self._action(edit_menu, "标记完成 / 取消", "Ctrl+Return", self.toggle_current_done)
        self._action(edit_menu, "插入表格", "", self.insert_table)
        self._action(edit_menu, "手绘笔迹…", "", self.open_sketch)

        view_menu = bar.addMenu("视图")
        self._action(view_menu, "聚焦搜索", "Ctrl+F", self.search_edit.setFocus)
        self._action(view_menu, "刷新", "F5", lambda: self.refresh_list(select_first=False))
        view_menu.addSeparator()
        group = QActionGroup(self)
        group.setExclusive(True)
        self.light_action = QAction("浅色主题", self, checkable=True)
        self.dark_action = QAction("深色主题", self, checkable=True)
        (self.dark_action if self.theme_name == "dark" else self.light_action).setChecked(True)
        self.light_action.triggered.connect(lambda: self.set_theme("light"))
        self.dark_action.triggered.connect(lambda: self.set_theme("dark"))
        group.addAction(self.light_action)
        group.addAction(self.dark_action)
        view_menu.addAction(self.light_action)
        view_menu.addAction(self.dark_action)

        help_menu = bar.addMenu("帮助")
        self._action(help_menu, "快捷键", "F1", self.show_shortcuts)
        self._action(help_menu, "关于墨记", "", self.show_about)

    def _action(self, menu: QMenu, text: str, shortcut: str | None, slot) -> QAction:
        action = QAction(text, self)
        if shortcut:
            action.setShortcut(QKeySequence(shortcut))
        action.triggered.connect(slot)
        menu.addAction(action)
        return action

    # ---------------------------------------------------------- 数据刷新
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
        if self.tag_filter:
            base = [e for e in base if self.tag_filter in e.tags]
        text = self.search_text.strip().lower()
        if text:
            terms = text.split()
            filtered = []
            for event in base:
                haystack = " ".join(
                    [event.title, event.note, " ".join(event.tags)]
                    + [a.name for a in event.attachments]
                ).lower()
                if all(term in haystack for term in terms):
                    filtered.append(event)
            base = filtered
        return base

    def refresh_list(self, select_first: bool = False) -> None:
        scroll_value = self.cards_scroll.verticalScrollBar().value()
        events = self.visible_events()
        while self.cards_layout.count():
            item = self.cards_layout.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.deleteLater()

        self._cards: dict[str, EventCard] = {}
        for event in events:
            card = EventCard(event, self.pal)
            card.clicked.connect(self.on_card_clicked)
            card.double_clicked.connect(self.on_card_double_clicked)
            card.toggled.connect(self.on_card_toggled)
            if event.id == self.current_id:
                card.set_selected(True)
            self._cards[event.id] = card
            self.cards_layout.addWidget(card)
        self.cards_layout.addStretch(1)

        self.empty_list_label.setVisible(not events)
        self.cards_scroll.setVisible(bool(events))
        ids = [e.id for e in events]
        if select_first and events and self.current_id not in ids:
            self.current_id = events[0].id
            self.load_editor(self.repo.get(self.current_id))
        elif not events:
            self.cards_scroll.setVisible(False)
            self.empty_list_label.show()

        self.count_label.setText(f"共 {len(events)} 条 · 按优先级排序")
        bits = []
        if self.filter_mode != "all":
            bits.append(f"视图：{self.nav_titles.get(self.filter_mode, self.filter_mode)}")
        if self.tag_filter:
            bits.append(f"标签：#{self.tag_filter}")
        if self.search_text:
            bits.append(f"搜索：{self.search_text}")
        self.filter_hint.setText("  ·  ".join(bits))
        self._update_stats()
        self._rebuild_tags()
        QTimer.singleShot(0, lambda: self.cards_scroll.verticalScrollBar().setValue(scroll_value))

    def _update_stats(self) -> None:
        stats = self.repo.stats()
        self.stat_today.set_value(str(stats["today"]), self.pal["primary"])
        self.stat_pending.set_value(str(stats["pending"]))
        self.stat_overdue.set_value(
            str(stats["overdue"]), self.pal["error"] if stats["overdue"] else self.pal["muted"]
        )
        self.stat_rate.set_value(stats["rate"], self.pal["success"])
        self.progress.setValue(int(str(stats["rate"]).rstrip("%") or 0))
        counts = {
            "all": stats["total"],
            "today": stats["today"],
            "pending": stats["pending"],
            "done": stats["done"],
            "overdue": stats["overdue"],
        }
        for key, button in self.nav_buttons.items():
            button.setText(f"{self.nav_titles[key]}   {counts.get(key, 0)}")
        self.setWindowTitle(f"{__app_name__} · {stats['pending']} 条待办")

    def _rebuild_tags(self) -> None:
        while self.tags_layout.count():
            item = self.tags_layout.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.deleteLater()
        tags = self.repo.tags()
        if not tags:
            empty = QLabel("暂无标签")
            empty.setProperty("role", "muted")
            empty.setStyleSheet("font-size: 9pt;")
            self.tags_layout.addWidget(empty)
        else:
            for tag, count in list(tags.items())[:20]:
                button = QPushButton(f"#{tag}  {count}")
                button.setObjectName("TagChip")
                button.setCheckable(True)
                button.setChecked(self.tag_filter == tag)
                button.setCursor(Qt.PointingHandCursor)
                button.setToolTip(f"{count} 条待办")
                button.setStyleSheet("text-align: left;")
                button.clicked.connect(
                    lambda checked=False, t=tag: self._on_tag_clicked(t, checked)
                )
                self.tags_layout.addWidget(button)


    def _on_tag_clicked(self, tag: str, checked: bool) -> None:
        self.flush_pending()
        self.tag_filter = tag if checked else None
        self.filter_mode = "all"
        self.nav_buttons["all"].setChecked(True)
        self.refresh_list(select_first=True)

    def set_filter(self, mode: str) -> None:
        self.flush_pending()
        self.filter_mode = mode
        self.tag_filter = None
        self.refresh_list(select_first=True)

    def _on_search_changed(self, text: str) -> None:
        self.search_text = text
        self._search_timer.start()

    def _do_search_refresh(self) -> None:
        self.flush_pending()
        self.refresh_list(select_first=True)

    def _update_card_selection(self) -> None:
        cards = getattr(self, "_cards", {})
        for event_id, card in cards.items():
            card.set_selected(event_id == self.current_id)

    # ---------------------------------------------------------- 编辑器
    def load_editor(self, event: Event | None) -> None:
        if event is None:
            self.editor_stack.setCurrentIndex(1)
            return
        self._loading = True
        try:
            self.editor_stack.setCurrentIndex(0)
            self.title_edit.setText(event.title)
            self.note_edit.setPlainText(event.note)
            self.due_check.setChecked(bool(event.due))
            if event.due:
                self.due_edit.setDateTime(QDateTime(event.due))
            else:
                self.due_edit.setDateTime(QDateTime(now() + timedelta(hours=1)))
            self.due_edit.setEnabled(bool(event.due))
            self.remind_spin.setValue(event.remind_before)
            self.tags_edit.setText(", ".join(event.tags))
            self._update_meta(event)
            self._load_attachments(event)
            self._update_done_button(event)
            try:
                stamp = (event.updated_at or now()).strftime("%H:%M")
            except Exception:
                stamp = now().strftime("%H:%M")
            self.status_label.setText(f"已保存 · {stamp}")
        finally:
            self._loading = False
        self._update_card_selection()

    def _update_meta(self, event: Event) -> None:
        bits = [f"P{event.priority}", f"截止 {humanize_due(event.due)}"]
        if event.tags:
            bits.append(" ".join("#" + t for t in event.tags))
        bits.append(f"创建于 {event.created_at:%Y-%m-%d %H:%M}" if event.created_at else "")
        if event.attachments:
            bits.append(f"附件 {len(event.attachments)}")
        self.meta_label.setText("  ·  ".join(b for b in bits if b))

    def _update_done_button(self, event: Event) -> None:
        self.done_button.setText("↺ 取消完成" if event.done else "✓ 标记完成")

    def _load_attachments(self, event: Event) -> None:
        self.att_list.clear()
        rows: list[tuple[str, str, str]] = []
        for att in event.attachments:
            try:
                absolute = str(self.repo.attachment_path(att))
            except Exception:
                absolute = ""
            rows.append((absolute, att.name, att.path))
        build_attachment_items(rows, self.att_list)

    def _schedule_save(self, *_args) -> None:
        if self._loading:
            return
        self._save_timer.start()

    def _gather(self) -> dict:
        title = self.title_edit.text().strip() or "未命名记事"
        tags = [
            t.strip().lstrip("#")
            for t in self.tags_edit.text().replace("，", ",").split(",")
            if t.strip()
        ]
        due = self.due_edit.dateTime().toPython() if self.due_check.isChecked() else None
        return {
            "title": title,
            "note": self.note_edit.toPlainText(),
            "due": due,
            "tags": tags,
            "remind_before": int(self.remind_spin.value()),
        }

    def save_current(self, show_toast: bool = False) -> bool:
        self._save_timer.stop()
        if self._loading or self._saving or not self.current_id:
            return True
        event = self.repo.get(self.current_id)
        if event is None:
            return True
        values = self._gather()
        changed = (
            values["title"] != event.title
            or values["note"] != event.note
            or values["due"] != event.due
            or values["tags"] != event.tags
            or values["remind_before"] != event.remind_before
        )
        if not changed:
            return True
        self._saving = True
        try:
            self.repo.update(event.id, **values)
        except RepoError as exc:
            self.toast.show_message("保存失败", str(exc), icon="⚠", duration=8000)
            return False
        finally:
            self._saving = False
        self._loading = True
        if self.title_edit.text().strip() != values["title"]:
            self.title_edit.setText(values["title"])
        self._loading = False
        updated = self.repo.get(event.id)
        if updated:
            self._update_meta(updated)
        self.status_label.setText(f"已保存 · {now():%H:%M}")
        self.refresh_list(select_first=False)
        if show_toast:
            self.toast.show_message("已保存", event.title, icon="✅", duration=2500)
        return True

    # ---------------------------------------------------------- 卡片事件
    def on_card_clicked(self, event_id: str) -> None:
        if event_id == self.current_id:
            return
        self.flush_pending()
        event = self.repo.get(event_id)
        if event is None:
            return
        self.current_id = event_id
        self.load_editor(event)

    def on_card_double_clicked(self, event_id: str) -> None:
        self.on_card_clicked(event_id)
        self.note_edit.setFocus()

    def on_card_toggled(self, event_id: str) -> None:
        self.flush_pending()
        try:
            event = self.repo.toggle_done(event_id)
        except RepoError as exc:
            self.toast.show_message("操作失败", str(exc), icon="⚠", duration=6000)
            return
        if event.id == self.current_id:
            self.load_editor(event)
        self.refresh_list(select_first=True)
        self.toast.show_message(
            "已完成" if event.done else "已恢复待办",
            event.title,
            icon="✅" if event.done else "↺",
            duration=2500,
        )

    def flush_pending(self) -> None:
        if self._save_timer.isActive():
            self.save_current()

    # ---------------------------------------------------------- 操作
    def new_event(self) -> None:
        self.flush_pending()
        self.filter_mode = "all"
        self.tag_filter = None
        self.search_text = ""
        self.search_edit.blockSignals(True)
        self.search_edit.clear()
        self.search_edit.blockSignals(False)
        self.nav_buttons["all"].setChecked(True)
        try:
            event = self.repo.add("新记事")
        except RepoError as exc:
            self.toast.show_message("新建失败", str(exc), icon="⚠", duration=6000)
            return
        self.current_id = event.id
        self.refresh_list(select_first=False)
        self.load_editor(event)
        self.title_edit.setFocus()
        self.title_edit.selectAll()

    def toggle_current_done(self) -> None:
        if not self.current_id:
            return
        self.flush_pending()
        try:
            event = self.repo.toggle_done(self.current_id)
        except RepoError as exc:
            self.toast.show_message("操作失败", str(exc), icon="⚠", duration=6000)
            return
        self.load_editor(event)
        self.refresh_list(select_first=True)
        self.toast.show_message(
            "已完成" if event.done else "已恢复待办", event.title,
            icon="✅" if event.done else "↺", duration=2500,
        )

    def duplicate_current(self) -> None:
        if not self.current_id:
            return
        self.flush_pending()
        try:
            event = self.repo.duplicate(self.current_id)
        except RepoError as exc:
            self.toast.show_message("复制失败", str(exc), icon="⚠", duration=6000)
            return
        self.current_id = event.id
        self.refresh_list(select_first=False)
        self.load_editor(event)
        self.toast.show_message("已复制", event.title, icon="⧉", duration=2500)

    def move_current(self, delta: int) -> None:
        if not self.current_id:
            return
        self.flush_pending()
        try:
            event = self.repo.move_by(self.current_id, delta)
        except RepoError as exc:
            self.toast.show_message("移动失败", str(exc), icon="⚠", duration=6000)
            return
        self.current_id = event.id
        self.refresh_list(select_first=False)
        self.load_editor(event)
        self.toast.show_message("优先级已调整", f"{event.title} → P{event.priority}", icon="↕", duration=2200)

    def delete_current(self) -> None:
        if not self.current_id:
            return
        event = self.repo.get(self.current_id)
        if event is None:
            return
        self.flush_pending()
        answer = QMessageBox.question(
            self,
            "删除记事",
            f"确定删除「{event.title}」吗？\n\n删除后仍可用 Ctrl+Alt+Z 撤销。",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        if answer != QMessageBox.Yes:
            return
        visible_ids = [e.id for e in self.visible_events()]
        try:
            self.repo.delete(event.id)
        except RepoError as exc:
            self.toast.show_message("删除失败", str(exc), icon="⚠", duration=6000)
            return
        next_id = None
        if event.id in visible_ids:
            index = visible_ids.index(event.id)
            remaining = visible_ids[:index] + visible_ids[index + 1:]
            next_id = remaining[min(index, len(remaining) - 1)] if remaining else None
        else:
            remaining = [e.id for e in self.visible_events() if e.id != event.id]
            next_id = remaining[0] if remaining else None
        self.current_id = next_id
        self.refresh_list(select_first=True)
        if self.current_id:
            self.load_editor(self.repo.get(self.current_id))
        else:
            self.editor_stack.setCurrentIndex(1)
        self.toast.show_message("已删除", event.title, icon="🗑", duration=3000)

    def undo_app(self) -> None:
        self.flush_pending()
        ok = self.repo.undo()
        self.current_id = self.current_id if self.repo.get(self.current_id or "") else None
        self.refresh_list(select_first=True)
        if self.current_id:
            self.load_editor(self.repo.get(self.current_id))
        self.toast.show_message(
            "已撤销" if ok else "没有可撤销的操作",
            "" if ok else "所有操作都已保存",
            icon="↶",
            duration=2500,
        )

    def redo_app(self) -> None:
        self.flush_pending()
        ok = self.repo.redo()
        self.current_id = self.current_id if self.repo.get(self.current_id or "") else None
        self.refresh_list(select_first=True)
        if self.current_id:
            self.load_editor(self.repo.get(self.current_id))
        self.toast.show_message(
            "已重做" if ok else "没有可重做的操作",
            "" if ok else "",
            icon="↷",
            duration=2500,
        )

    def _on_due_toggled(self, checked: bool) -> None:
        self.due_edit.setEnabled(checked)
        if checked and not self._loading:
            self.due_edit.setDateTime(QDateTime(now() + timedelta(hours=1)))
        self._schedule_save()

    def insert_table(self) -> None:
        if not self.current_id:
            return
        snippet = "\n| 项目 | 内容 |\n| --- | --- |\n|  |  |\n"
        cursor = self.note_edit.textCursor()
        cursor.insertText(snippet)
        self.note_edit.setFocus()

    # ---------------------------------------------------------- 附件
    def attach_files(self, paths: list[str]) -> None:
        if not self.current_id or not paths:
            return
        self.flush_pending()
        added = 0
        errors = []
        for path in paths:
            try:
                self.repo.add_attachment(self.current_id, path)
                added += 1
            except RepoError as exc:
                errors.append(f"{Path(path).name}: {exc}")
        event = self.repo.get(self.current_id)
        if event:
            self.load_editor(event)
        if added:
            self.refresh_list(select_first=False)
            self.toast.show_message("已添加附件", f"{added} 个文件", icon="⧉", duration=2500)
        if errors:
            self.toast.show_message("部分附件添加失败", "\n".join(errors[:3]), icon="⚠", duration=7000)

    def attach_image(self, image) -> None:
        if not self.current_id:
            return
        fd, path = tempfile.mkstemp(prefix="moji-paste-", suffix=".png")
        os.close(fd)
        try:
            if not image.save(path, "PNG"):
                raise OSError("图片编码失败")
            self.repo.add_attachment(self.current_id, path)
        except Exception as exc:
            self.toast.show_message("图片粘贴失败", str(exc), icon="⚠", duration=6000)
            return
        finally:
            try:
                os.unlink(path)
            except OSError:
                pass
        event = self.repo.get(self.current_id)
        if event:
            self.load_editor(event)
        self.refresh_list(select_first=False)
        self.toast.show_message("图片已存为附件", "", icon="🖼", duration=2200)

    def choose_attachments(self) -> None:
        if not self.current_id:
            return
        paths, _ = QFileDialog.getOpenFileNames(
            self,
            "添加附件",
            str(Path.home()),
            "所有文件 (*.*);;图片 (*.png *.jpg *.jpeg *.gif *.webp *.svg);;音频 (*.mp3 *.wav *.ogg *.m4a)",
        )
        if paths:
            self.attach_files(paths)

    def open_attachment(self, item: QListWidgetItem) -> None:
        rel = item.data(Qt.UserRole)
        if rel:
            self.open_attachment_path(str(rel))

    def open_attachment_path(self, rel: str) -> None:
        try:
            path = self.repo.store.resolve(rel)
        except Exception as exc:
            self.toast.show_message("附件路径异常", str(exc), icon="⚠", duration=6000)
            return
        if not path.is_file():
            self.toast.show_message("附件文件丢失", rel, icon="⚠", duration=6000)
            return
        QDesktopServices.openUrl(QUrl.fromLocalFile(str(path)))

    def remove_attachment(self, index: int) -> None:
        if not self.current_id:
            return
        self.flush_pending()
        try:
            att = self.repo.remove_attachment(self.current_id, index)
        except RepoError as exc:
            self.toast.show_message("移除失败", str(exc), icon="⚠", duration=6000)
            return
        event = self.repo.get(self.current_id)
        if event:
            self.load_editor(event)
        self.refresh_list(select_first=False)
        self.toast.show_message("已移除附件", att.name, icon="⧉", duration=2500)

    def reorder_attachments(self, rel_paths: list[str]) -> None:
        if not self.current_id:
            return
        event = self.repo.get(self.current_id)
        if not event:
            return
        try:
            self.repo.reorder_attachments(self.current_id, rel_paths)
        except RepoError as exc:
            self.toast.show_message("排序失败", str(exc), icon="⚠", duration=6000)
            self.load_editor(event)
            return
        self.status_label.setText("附件顺序已保存")
        self.refresh_list(select_first=False)

    def open_sketch(self) -> None:
        if not self.current_id:
            return
        dialog = SketchDialog(self)
        if dialog.exec() == QDialog.Accepted and dialog.image_path:
            path = dialog.image_path
            try:
                self.repo.add_attachment(self.current_id, path)
            except RepoError as exc:
                self.toast.show_message("保存笔迹失败", str(exc), icon="⚠", duration=6000)
                return
            finally:
                try:
                    os.unlink(path)
                except OSError:
                    pass
            event = self.repo.get(self.current_id)
            if event:
                self.load_editor(event)
            self.refresh_list(select_first=False)
            self.toast.show_message("笔迹已保存为附件", "", icon="✎", duration=2200)

    # ---------------------------------------------------------- 导入导出
    def export_data(self) -> None:
        self.flush_pending()
        default = str(Path.home() / f"moji-export-{now():%Y%m%d}.mjnote")
        path, selected = QFileDialog.getSaveFileName(
            self,
            "导出记事",
            default,
            "墨记打包 (*.mjnote);;JSON (*.json);;todo.txt (*.txt);;Markdown (*.md)",
        )
        if not path:
            return
        path = self._ensure_suffix(path, selected)
        try:
            out = importer.export_auto(self.repo, path)
        except RepoError as exc:
            QMessageBox.critical(self, "导出失败", str(exc))
            return
        self.toast.show_message("导出完成", str(out), icon="⬇", duration=5000)

    def import_data(self) -> None:
        self.flush_pending()
        path, _ = QFileDialog.getOpenFileName(
            self,
            "导入记事",
            str(Path.home()),
            "墨记数据 (*.mjnote *.json *.txt);;所有文件 (*.*)",
        )
        if not path:
            return
        box = QMessageBox(self)
        box.setWindowTitle("导入记事")
        box.setText(f"从\n{path}\n导入数据？")
        box.setInformativeText("「合并」会保留现有记事并跳过重复项；「清空并导入」会先删除现有数据。")
        merge_button = box.addButton("合并导入", QMessageBox.AcceptRole)
        replace_button = box.addButton("清空并导入", QMessageBox.DestructiveRole)
        box.addButton("取消", QMessageBox.RejectRole)
        box.exec()
        if box.clickedButton() is merge_button:
            merge = True
        elif box.clickedButton() is replace_button:
            merge = False
        else:
            return
        try:
            report = importer.import_auto(self.repo, path, merge=merge)
        except RepoError as exc:
            QMessageBox.critical(self, "导入失败", str(exc))
            return
        self.current_id = None
        self.refresh_list(select_first=True)
        if self.current_id:
            self.load_editor(self.repo.get(self.current_id))
        else:
            self.editor_stack.setCurrentIndex(1)
        self.toast.show_message(
            "导入完成", f"新增 {report['added']} 条，跳过 {report['skipped']} 条",
            icon="⬆", duration=5000,
        )

    @staticmethod
    def _ensure_suffix(path: str, selected_filter: str) -> str:
        p = Path(path)
        if p.suffix:
            return path
        mapping = {
            "墨记打包 (*.mjnote)": ".mjnote",
            "JSON (*.json)": ".json",
            "todo.txt (*.txt)": ".txt",
            "Markdown (*.md)": ".md",
        }
        return str(p.with_suffix(mapping.get(selected_filter, ".mjnote")))

    # ---------------------------------------------------------- 主题
    def toggle_theme(self) -> None:
        self.set_theme("light" if self.theme_name == "dark" else "dark")

    def set_theme(self, name: str) -> None:
        self.theme_name = name
        self.pal = palette(name)
        QApplication.instance().setStyleSheet(build_qss(name))
        self.light_action.setChecked(name == "light")
        self.dark_action.setChecked(name == "dark")
        self.theme_button.setText("☀ 浅色" if name == "dark" else "🌙 深色")
        self.refresh_list(select_first=False)
        event = self.repo.get(self.current_id) if self.current_id else None
        if event:
            self.load_editor(event)
        cfg = self.repo.store.load_config()
        cfg["theme"] = name
        self.repo.store.save_config(cfg)
        self.toast.show_message("已切换主题", "深色模式" if name == "dark" else "浅色模式", icon="🎨", duration=2000)

    # ---------------------------------------------------------- 提醒 / 托盘
    def _greet_today(self) -> None:
        today = [e for e in self.repo.today() if not e.done]
        if not today:
            return
        QApplication.beep()
        self.toast.show_message(
            "今日安排",
            f"今天有 {len(today)} 项待办，第一项：{today[0].title}",
            icon="📅",
            duration=7000,
        )

    def check_reminders(self) -> None:
        due = self.repo.due_reminders(now())
        if not due:
            return
        QApplication.beep()
        text = reminders.reminder_text(due)
        self.toast.show_message("⏰ 待办提醒", text, icon="⏰", duration=12000)
        if getattr(self, "tray", None) is not None:
            try:
                self.tray.showMessage(
                    "墨记 · 待办提醒", text, QSystemTrayIcon.Information, 8000
                )
            except Exception:
                pass
        try:
            self.repo.mark_reminded(due)
        except Exception:
            pass

    def _setup_tray(self) -> None:
        self.tray: QSystemTrayIcon | None = None
        if not QSystemTrayIcon.isSystemTrayAvailable():
            return
        try:
            tray = QSystemTrayIcon(make_app_icon(64), self)
            tray.setToolTip(f"{__app_name__}")
            menu = QMenu()
            show_action = menu.addAction("显示主窗口")
            show_action.triggered.connect(self._restore_window)
            new_action = menu.addAction("新建记事")
            new_action.triggered.connect(self.new_event)
            menu.addSeparator()
            quit_action = menu.addAction("退出")
            quit_action.triggered.connect(self.close)
            tray.setContextMenu(menu)
            tray.activated.connect(self._on_tray_activated)
            tray.show()
            self.tray = tray
            self.tray_menu = menu  # 防止 QMenu 被 Python 回收
        except Exception:
            self.tray = None

    def _on_tray_activated(self, reason) -> None:
        if reason in (QSystemTrayIcon.DoubleClick, QSystemTrayIcon.Trigger):
            self._restore_window()

    def _restore_window(self) -> None:
        self.showNormal()
        self.raise_()
        self.activateWindow()

    # ---------------------------------------------------------- 杂项
    def _open_data_dir(self) -> None:
        QDesktopServices.openUrl(QUrl.fromLocalFile(str(self.repo.store.dir)))

    def show_shortcuts(self) -> None:
        QMessageBox.information(
            self,
            "快捷键",
            "<b>常用快捷键</b><br><br>"
            "Ctrl+N　新建记事<br>"
            "Ctrl+S　保存当前<br>"
            "Ctrl+F　聚焦搜索<br>"
            "Ctrl+O　导入　Ctrl+Shift+S　导出<br>"
            "Ctrl+Alt+Z / Ctrl+Alt+Y　撤销 / 重做应用操作<br>"
            "Ctrl+Return　标记完成<br>"
            "F5　刷新　F1　本帮助<br><br>"
            "<b>附件</b><br>"
            "把文件拖进备注或附件区即可添加；<br>"
            "在附件区拖动图标可调整顺序，双击打开，右键移除。<br>"
            "备注里 Ctrl+V 可直接粘贴剪贴板图片。",
        )

    def show_about(self) -> None:
        QMessageBox.about(
            self,
            "关于墨记",
            f"<h3>{__app_name__} v{__version__}</h3>"
            "<p>课程设计《电子记事本的实现》—— TUI / GUI 双界面电子记事本。</p>"
            "<p>核心：优先级单向链表、UTF-8 文本存储、自动备份、撤销重做、"
            "附件与手绘、todo.txt / Markdown / JSON / 打包导入导出。</p>"
            f"<p style='color:#888'>数据目录：{self.repo.store.dir}</p>",
        )

    def _restore_geometry(self, cfg: dict | None = None) -> None:
        cfg = cfg or self.repo.store.load_config()
        window = cfg.get("window") or {}
        try:
            width = max(1020, int(window.get("w", 1180)))
            height = max(660, int(window.get("h", 760)))
        except (TypeError, ValueError):
            width, height = 1180, 760
        self.resize(width, height)

    def closeEvent(self, event) -> None:  # type: ignore[override]
        self._save_timer.stop()
        self._search_timer.stop()
        if not self.save_current():
            answer = QMessageBox.question(
                self,
                "保存失败",
                "当前记事保存失败，仍要退出吗？\n未保存的修改可能会丢失。",
                QMessageBox.Yes | QMessageBox.No,
                QMessageBox.No,
            )
            if answer != QMessageBox.Yes:
                event.ignore()
                return
        cfg = self.repo.store.load_config()
        cfg["theme"] = self.theme_name
        cfg["window"] = {"w": self.width(), "h": self.height()}
        self.repo.store.save_config(cfg)
        if getattr(self, "tray", None) is not None:
            self.tray.hide()
        super().closeEvent(event)


def launch_gui(repo: Repository) -> int:
    app = QApplication.instance() or QApplication(sys.argv)
    app.setApplicationName(__app_name__)
    app.setApplicationDisplayName(__app_name__)
    app.setWindowIcon(make_app_icon())
    app.setStyle("Fusion")
    font = QFont("Microsoft YaHei UI", 10)
    app.setFont(font)
    shot = os.environ.get("MOJI_SHOT")
    window = MainWindow(repo)
    if shot:
        # 离屏截图：使用真实平台字体，但不真正显示窗口
        window.setAttribute(Qt.WA_DontShowOnScreen, True)
    window.show()

    if shot:
        def snap() -> None:
            try:
                window.grab().save(shot, "PNG")
            finally:
                QApplication.quit()

        QTimer.singleShot(1600, snap)
    return app.exec()

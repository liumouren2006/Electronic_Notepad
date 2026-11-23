"""GUI 自定义控件：事件卡片、统计卡、Toast、附件板、手绘板。"""

from __future__ import annotations

import os
import tempfile
from pathlib import Path
from typing import Iterable

from PySide6.QtCore import QFileInfo, QPoint, QSize, Qt, QTimer, Signal
from PySide6.QtGui import (
    QColor,
    QFont,
    QIcon,
    QMouseEvent,
    QPainter,
    QPen,
    QPixmap,
)
from PySide6.QtWidgets import (
    QCheckBox,
    QDialog,
    QFileIconProvider,
    QFrame,
    QGraphicsOpacityEffect,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QMenu,
    QPushButton,
    QSizePolicy,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

from ..core.models import Event, humanize_due
from .theme import chip_style

PRIORITY_COLORS = [
    ("#ef4444", "#fee2e2"),
    ("#f97316", "#ffedd5"),
    ("#eab308", "#fef9c3"),
    ("#22c55e", "#dcfce7"),
    ("#06b6d4", "#cffafe"),
    ("#6366f1", "#e0e7ff"),
]


class ElidedLabel(QLabel):
    """自动省略号标题，避免长标题把卡片撑开。"""

    def __init__(self, text: str = "", parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._full = text or ""
        self.setToolTip(self._full)
        self.setMinimumWidth(40)
        self.setSizePolicy(QSizePolicy.Ignored, QSizePolicy.Preferred)
        self.setAttribute(Qt.WA_TransparentForMouseEvents, True)

    def setText(self, text: str) -> None:  # type: ignore[override]
        self._full = text or ""
        self.setToolTip(self._full)
        self._apply()

    def full_text(self) -> str:
        return self._full

    def resizeEvent(self, event) -> None:  # type: ignore[no-untyped-def]
        super().resizeEvent(event)
        self._apply()

    def _apply(self) -> None:
        metrics = self.fontMetrics()
        width = max(10, self.width() - 2)
        super().setText(metrics.elidedText(self._full, Qt.ElideRight, width))


def _make_chip(text: str, color: str, bg: str) -> QLabel:
    label = QLabel(text)
    label.setStyleSheet(chip_style(color, bg))
    label.setAttribute(Qt.WA_TransparentForMouseEvents, True)
    return label


class EventCard(QFrame):
    clicked = Signal(str)
    double_clicked = Signal(str)
    toggled = Signal(str)

    def __init__(self, event: Event, palette: dict, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.event_id = event.id
        self.palette = palette
        self.setObjectName("Card")
        self.setProperty("selected", False)
        self.setCursor(Qt.PointingHandCursor)
        self.setFixedHeight(88)
        self._build(event)

    def _build(self, event: Event) -> None:
        pal = self.palette
        layout = QHBoxLayout(self)
        layout.setContentsMargins(12, 10, 14, 10)
        layout.setSpacing(11)

        if event.done:
            accent_color = pal["muted"]
        elif event.is_overdue:
            accent_color = pal["error"]
        elif event.color:
            accent_color = event.color
        else:
            accent_color = PRIORITY_COLORS[(event.priority - 1) % len(PRIORITY_COLORS)][0]
        accent = QLabel()
        accent.setFixedSize(4, 48)
        accent.setStyleSheet(f"background: {accent_color}; border-radius: 2px;")
        accent.setAttribute(Qt.WA_TransparentForMouseEvents, True)
        layout.addWidget(accent)

        check = QCheckBox()
        check.setChecked(event.done)
        check.setToolTip("标记完成 / 取消完成")
        check.setCursor(Qt.PointingHandCursor)
        check.toggled.connect(lambda _checked: self.toggled.emit(self.event_id))
        layout.addWidget(check, 0, Qt.AlignTop)

        body = QVBoxLayout()
        body.setSpacing(5)
        body.setContentsMargins(0, 0, 0, 0)
        title = ElidedLabel(event.title)
        font = QFont(title.font())
        font.setPointSizeF(10.8)
        font.setBold(True)
        font.setStrikeOut(event.done)
        title.setFont(font)
        title.setStyleSheet(f"color: {pal['muted'] if event.done else pal['text']};")
        body.addWidget(title)

        meta = QHBoxLayout()
        meta.setSpacing(6)
        meta.setContentsMargins(0, 0, 0, 0)
        if event.done:
            meta.addWidget(_make_chip("已完成", pal["muted"], pal["surfaceAlt"]))
        elif event.due:
            color = pal["error"] if event.is_overdue else (
                pal["warning"] if event.due_today else pal["muted"]
            )
            bg = pal["errorSoft"] if event.is_overdue else (
                pal["warningSoft"] if event.due_today else pal["surfaceAlt"]
            )
            meta.addWidget(_make_chip("⏱ " + humanize_due(event.due), color, bg))
        else:
            meta.addWidget(_make_chip("无期限", pal["muted"], pal["surfaceAlt"]))
        p_color, p_bg = PRIORITY_COLORS[(event.priority - 1) % len(PRIORITY_COLORS)]
        meta.addWidget(_make_chip(f"P{event.priority}", p_color, p_bg))
        for tag in event.tags[:2]:
            text = "#" + (tag if len(tag) <= 10 else tag[:9] + "…")
            label = QLabel(text)
            label.setStyleSheet(
                f"color: {pal['violet']}; font-size: 9pt; padding: 0 2px;"
            )
            label.setAttribute(Qt.WA_TransparentForMouseEvents, True)
            meta.addWidget(label)
        meta.addStretch(1)
        if event.attachments:
            att = QLabel(f"⧉ {len(event.attachments)}")
            att.setStyleSheet(f"color: {pal['teal']}; font-size: 9pt;")
            att.setToolTip("附件：" + "、".join(a.name for a in event.attachments))
            att.setAttribute(Qt.WA_TransparentForMouseEvents, True)
            meta.addWidget(att)
        body.addLayout(meta)
        layout.addLayout(body, 1)

    def set_selected(self, selected: bool) -> None:
        if self.property("selected") == selected:
            return
        self.setProperty("selected", selected)
        self.style().unpolish(self)
        self.style().polish(self)
        self.update()

    def mousePressEvent(self, event: QMouseEvent) -> None:  # type: ignore[override]
        if event.button() == Qt.LeftButton:
            self.clicked.emit(self.event_id)
        super().mousePressEvent(event)

    def mouseDoubleClickEvent(self, event: QMouseEvent) -> None:  # type: ignore[override]
        if event.button() == Qt.LeftButton:
            self.double_clicked.emit(self.event_id)
        super().mouseDoubleClickEvent(event)


class StatCard(QFrame):
    def __init__(self, caption: str, value: str = "0", parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("StatCard")
        self.setMinimumHeight(58)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 7, 10, 7)
        layout.setSpacing(0)
        self.value_label = QLabel(value)
        self.value_label.setProperty("role", "statValue")
        self.caption_label = QLabel(caption)
        self.caption_label.setProperty("role", "statCaption")
        layout.addWidget(self.value_label)
        layout.addWidget(self.caption_label)

    def set_value(self, value: str, color: str | None = None) -> None:
        self.value_label.setText(value)
        if color:
            self.value_label.setStyleSheet(f"color: {color};")
        else:
            self.value_label.setStyleSheet("")


class Toast(QFrame):
    """右下角浮出的 Snackbar 式提示。"""

    def __init__(self, parent: QWidget) -> None:
        super().__init__(parent)
        self.setObjectName("Toast")
        self.setAttribute(Qt.WA_TransparentForMouseEvents, True)
        self.hide()
        self._effect = QGraphicsOpacityEffect(self)
        self._effect.setOpacity(0.0)
        self.setGraphicsEffect(self._effect)
        self._timer = QTimer(self)
        self._timer.setSingleShot(True)
        self._timer.timeout.connect(self._fade_out)

        layout = QHBoxLayout(self)
        layout.setContentsMargins(16, 12, 18, 12)
        layout.setSpacing(10)
        self.icon = QLabel("⏰")
        self.icon.setObjectName("ToastIcon")
        layout.addWidget(self.icon, 0, Qt.AlignTop)
        body = QVBoxLayout()
        body.setSpacing(2)
        self.title = QLabel("")
        self.title.setObjectName("ToastTitle")
        self.body = QLabel("")
        self.body.setObjectName("ToastBody")
        self.body.setWordWrap(True)
        body.addWidget(self.title)
        body.addWidget(self.body)
        layout.addLayout(body, 1)

    def show_message(
        self,
        title: str,
        body: str = "",
        icon: str = "⏰",
        duration: int = 6000,
    ) -> None:
        self.title.setText(title)
        self.body.setText(body)
        self.body.setVisible(bool(body))
        self.icon.setText(icon)
        self.setFixedWidth(min(380, max(240, self.parentWidget().width() // 3)))
        self.adjustSize()
        self._place()
        self._effect.setOpacity(0.0)
        self.show()
        self.raise_()
        self._fade(0.0, 1.0, 180)
        self._timer.start(duration)

    def _place(self) -> None:
        parent = self.parentWidget()
        if parent is None:
            return
        x = max(16, parent.width() - self.width() - 26)
        y = max(16, parent.height() - self.height() - 26)
        self.move(x, y)

    def _fade(self, start: float, end: float, ms: int) -> None:
        from PySide6.QtCore import QEasingCurve, QPropertyAnimation

        anim = QPropertyAnimation(self._effect, b"opacity", self)
        anim.setDuration(ms)
        anim.setStartValue(start)
        anim.setEndValue(end)
        anim.setEasingCurve(QEasingCurve.OutCubic)
        anim.start(QPropertyAnimation.DeleteWhenStopped)
        self._anim = anim

    def _fade_out(self) -> None:
        self._fade(1.0, 0.0, 220)
        QTimer.singleShot(240, self.hide)


class AttachmentList(QListWidget):
    """附件坞：支持拖入文件、内部拖动排序、双击打开、右键移除。"""

    files_dropped = Signal(list)
    reordered = Signal(list)
    remove_requested = Signal(int)
    open_requested = Signal(str)

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("Attachments")
        self.setViewMode(QListWidget.IconMode)
        self.setIconSize(QSize(76, 56))
        self.setGridSize(QSize(104, 104))
        self.setResizeMode(QListWidget.Adjust)
        self.setMovement(QListWidget.Snap)
        self.setSpacing(4)
        self.setWordWrap(True)
        self.setTextElideMode(Qt.ElideMiddle)
        self.setDragDropMode(QListWidget.InternalMove)
        self.setDefaultDropAction(Qt.MoveAction)
        self.setSelectionMode(QListWidget.SingleSelection)
        self.setMinimumHeight(118)
        self.setMaximumHeight(150)

    def dragEnterEvent(self, event) -> None:  # type: ignore[no-untyped-def]
        if event.mimeData().hasUrls():
            event.acceptProposedAction()
        else:
            super().dragEnterEvent(event)

    def dragMoveEvent(self, event) -> None:  # type: ignore[no-untyped-def]
        if event.mimeData().hasUrls():
            event.acceptProposedAction()
        else:
            super().dragMoveEvent(event)

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
        self.reordered.emit(self.paths())

    def paths(self) -> list[str]:
        return [
            self.item(i).data(Qt.UserRole)
            for i in range(self.count())
            if self.item(i).data(Qt.UserRole)
        ]

    def contextMenuEvent(self, event) -> None:  # type: ignore[no-untyped-def]
        item = self.itemAt(event.pos())
        if item is None:
            return
        menu = QMenu(self)
        open_action = menu.addAction("打开")
        remove_action = menu.addAction("从记事中移除")
        chosen = menu.exec(event.globalPos())
        if chosen == open_action:
            path = item.data(Qt.UserRole)
            if path:
                self.open_requested.emit(str(path))
        elif chosen == remove_action:
            self.remove_requested.emit(self.row(item))


def build_attachment_items(
    rows: Iterable[tuple], list_widget: AttachmentList, icon_size: int = 76
) -> None:
    """rows 每项为 (用于取图标的绝对路径, 显示名, 保存用的相对路径)。"""
    provider = QFileIconProvider()

    for row in rows:
        path, name = row[0], row[1]
        data = row[2] if len(row) > 2 else path
        item = QListWidgetItem(name)
        item.setData(Qt.UserRole, data)
        item.setToolTip(path or name)
        icon: QIcon
        if path and Path(path).is_file():
            pix = QPixmap(path)
            if not pix.isNull():
                icon = QIcon(
                    pix.scaled(
                        QSize(icon_size, 56),
                        Qt.KeepAspectRatio,
                        Qt.SmoothTransformation,
                    )
                )
            else:
                icon = provider.icon(QFileInfo(path))
        else:
            icon = provider.icon(QFileInfo(name))
        item.setIcon(icon)
        item.setTextAlignment(Qt.AlignHCenter | Qt.AlignBottom)
        list_widget.addItem(item)


class SketchCanvas(QWidget):
    """最简单的笔迹画板：鼠标拖动即落笔。"""

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setFixedSize(880, 500)
        self._pixmap = QPixmap(self.size())
        self._pixmap.fill(QColor("#ffffff"))
        self._undo: list[QPixmap] = []
        self._last: QPoint | None = None
        self.pen_color = QColor("#1f2430")
        self.pen_width = 4
        self._drawing = False

    def paintEvent(self, event) -> None:  # type: ignore[no-untyped-def]
        painter = QPainter(self)
        painter.drawPixmap(0, 0, self._pixmap)

    def _push_undo(self) -> None:
        self._undo.append(self._pixmap.copy())
        del self._undo[:-20]

    def mousePressEvent(self, event: QMouseEvent) -> None:  # type: ignore[override]
        if event.button() != Qt.LeftButton:
            return
        self._push_undo()
        self._drawing = True
        self._last = event.position().toPoint()
        painter = QPainter(self._pixmap)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setPen(QPen(self.pen_color, self.pen_width, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin))
        painter.drawPoint(self._last)
        painter.end()
        self.update()

    def mouseMoveEvent(self, event: QMouseEvent) -> None:  # type: ignore[override]
        if not self._drawing or self._last is None:
            return
        point = event.position().toPoint()
        painter = QPainter(self._pixmap)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setPen(QPen(self.pen_color, self.pen_width, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin))
        painter.drawLine(self._last, point)
        painter.end()
        self._last = point
        self.update()

    def mouseReleaseEvent(self, event: QMouseEvent) -> None:  # type: ignore[override]
        self._drawing = False
        self._last = None

    def undo(self) -> None:
        if self._undo:
            self._pixmap = self._undo.pop()
            self.update()

    def clear(self) -> None:
        self._push_undo()
        self._pixmap.fill(QColor("#ffffff"))
        self.update()

    def save_png(self) -> str:
        fd, path = tempfile.mkstemp(prefix="moji-sketch-", suffix=".png")
        os.close(fd)
        self._pixmap.save(path, "PNG")
        return path


class SketchDialog(QDialog):
    SWATCHES = ["#1f2430", "#ef4444", "#4f6ef7", "#16a34a", "#f97316", "#7c5cff"]

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setWindowTitle("手绘笔迹")
        self.setModal(True)
        self.image_path: str | None = None
        layout = QVBoxLayout(self)
        layout.setContentsMargins(16, 14, 16, 14)
        layout.setSpacing(10)

        toolbar = QHBoxLayout()
        toolbar.setSpacing(8)
        toolbar.addWidget(QLabel("颜色"))
        self._swatch_buttons = []
        for color in self.SWATCHES:
            button = QPushButton()
            button.setFixedSize(24, 24)
            button.setCursor(Qt.PointingHandCursor)
            button.setStyleSheet(
                f"background: {color}; border-radius: 12px; border: 2px solid transparent;"
            )
            button.clicked.connect(lambda _=False, c=color: self._set_color(c))
            self._swatch_buttons.append(button)
            toolbar.addWidget(button)
        toolbar.addSpacing(10)
        toolbar.addWidget(QLabel("粗细"))
        self.width_spin = QSpinBox()
        self.width_spin.setRange(1, 18)
        self.width_spin.setValue(4)
        self.width_spin.setFixedWidth(64)
        self.width_spin.valueChanged.connect(self._set_width)
        toolbar.addWidget(self.width_spin)
        toolbar.addStretch(1)
        undo_button = QPushButton("↶ 撤销")
        undo_button.setObjectName("Ghost")
        undo_button.clicked.connect(lambda: self.canvas.undo())
        clear_button = QPushButton("清空")
        clear_button.setObjectName("Ghost")
        clear_button.clicked.connect(lambda: self.canvas.clear())
        toolbar.addWidget(undo_button)
        toolbar.addWidget(clear_button)
        layout.addLayout(toolbar)

        self.canvas = SketchCanvas(self)
        layout.addWidget(self.canvas)

        buttons = QHBoxLayout()
        hint = QLabel("画好的笔迹会作为 PNG 附件保存到当前记事")
        hint.setProperty("role", "muted")
        buttons.addWidget(hint)
        buttons.addStretch(1)
        cancel = QPushButton("取消")
        cancel.clicked.connect(self.reject)
        save = QPushButton("保存为附件")
        save.setObjectName("Primary")
        save.clicked.connect(self._save)
        buttons.addWidget(cancel)
        buttons.addWidget(save)
        layout.addLayout(buttons)
        self._set_color(self.SWATCHES[0])

    def _set_color(self, color: str) -> None:
        self.canvas.pen_color = QColor(color)
        for button, swatch in zip(self._swatch_buttons, self.SWATCHES):
            border = "2px solid #4f6ef7" if swatch == color else "2px solid transparent"
            button.setStyleSheet(f"background: {swatch}; border-radius: 12px; border: {border};")

    def _set_width(self, value: int) -> None:
        self.canvas.pen_width = value

    def _save(self) -> None:
        self.image_path = self.canvas.save_png()
        self.accept()

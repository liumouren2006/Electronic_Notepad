"""MD 风格配色与 QSS。"""

from __future__ import annotations

from pathlib import Path
from string import Template

from moji import __version__

ASSETS = Path(__file__).resolve().parent.parent / "assets"

LIGHT = {
    "name": "light",
    "bg": "#f4f6fb",
    "surface": "#ffffff",
    "surfaceAlt": "#eef1f8",
    "surfaceHover": "#e6ebf7",
    "text": "#1c2130",
    "muted": "#68708a",
    "primary": "#4f6ef7",
    "primaryHover": "#3f5ce0",
    "primarySoft": "#e7ecff",
    "border": "#dfe4f0",
    "input": "#f8f9fd",
    "success": "#16a34a",
    "successSoft": "#e7f7ee",
    "warning": "#d97706",
    "warningSoft": "#fef3e2",
    "error": "#dc2626",
    "errorSoft": "#fdecec",
    "violet": "#7c5cff",
    "teal": "#0e9488",
    "toastBg": "#1c2130",
    "toastText": "#f5f7ff",
    "toastMuted": "#b7bfd4",
    "cardShadow": "rgba(31, 41, 74, 0.06)",
}

DARK = {
    "name": "dark",
    "bg": "#0e1117",
    "surface": "#161a23",
    "surfaceAlt": "#1d2230",
    "surfaceHover": "#242a3a",
    "text": "#e8eaf2",
    "muted": "#939cb3",
    "primary": "#7c9cff",
    "primaryHover": "#94afff",
    "primarySoft": "#232f55",
    "border": "#2a3142",
    "input": "#11151d",
    "success": "#4ade80",
    "successSoft": "#16321f",
    "warning": "#fbbf24",
    "warningSoft": "#3a2c12",
    "error": "#f87171",
    "errorSoft": "#3a1c1f",
    "violet": "#a78bfa",
    "teal": "#2dd4bf",
    "toastBg": "#242a3a",
    "toastText": "#f5f7ff",
    "toastMuted": "#aab3c8",
    "cardShadow": "rgba(0, 0, 0, 0.35)",
}


def palette(name: str) -> dict:
    return DARK if str(name).lower() == "dark" else LIGHT


QSS = Template(
    """
* {
    font-family: "Microsoft YaHei UI", "Microsoft YaHei", "Segoe UI", sans-serif;
    color: $text;
    outline: none;
}
QMainWindow, QWidget#Root { background: $bg; }
QToolTip {
    background: $toastBg; color: $toastText; border: none;
    padding: 6px 10px; border-radius: 6px;
}

QFrame#Sidebar {
    background: $surface;
    border-right: 1px solid $border;
}
QFrame#CenterPanel { background: $bg; }
QFrame#EditorPanel { background: $surface; border-left: 1px solid $border; }

QLabel[role="muted"] { color: $muted; }
QLabel[role="h1"] { font-size: 21px; font-weight: 700; }
QLabel[role="h2"] { font-size: 15px; font-weight: 700; }
QLabel[role="h3"] { font-size: 12px; font-weight: 700; color: $muted; letter-spacing: 1px; }
QLabel[role="statValue"] { font-size: 19px; font-weight: 700; }
QLabel[role="statCaption"] { font-size: 10px; color: $muted; }

QFrame#StatCard {
    background: $surfaceAlt;
    border-radius: 12px;
}
QFrame#BrandMark {
    background: $primary;
    border-radius: 16px;
}
QLabel#BrandMarkText { color: #ffffff; font-size: 24px; font-weight: 700; }

QPushButton {
    background: $surfaceAlt;
    border: 1px solid $border;
    border-radius: 10px;
    padding: 7px 14px;
    font-size: 10.5pt;
}
QPushButton:hover { background: $surfaceHover; }
QPushButton:pressed { background: $border; }
QPushButton:disabled { color: $muted; background: $surfaceAlt; }
QPushButton#Primary {
    background: $primary;
    color: #ffffff;
    border: none;
    font-weight: 600;
    padding: 8px 18px;
}
QPushButton#Primary:hover { background: $primaryHover; }
QPushButton#Danger { color: $error; background: transparent; border: 1px solid $error; }
QPushButton#Danger:hover { background: $errorSoft; }
QPushButton#Ghost {
    background: transparent;
    border: none;
    color: $muted;
    padding: 6px 10px;
}
QPushButton#Ghost:hover { background: $surfaceHover; color: $text; }
QPushButton#NavButton {
    text-align: left;
    background: transparent;
    border: none;
    color: $muted;
    padding: 7px 12px;
    border-radius: 10px;
    font-size: 10.5pt;
}
QPushButton#NavButton:hover { background: $surfaceHover; color: $text; }
QPushButton#NavButton:checked {
    background: $primarySoft;
    color: $primary;
    font-weight: 600;
}
QPushButton#TagChip {
    background: $surfaceAlt;
    color: $muted;
    border: 1px solid transparent;
    border-radius: 9px;
    padding: 3px 10px;
    font-size: 9.5pt;
}
QPushButton#TagChip:hover { color: $text; background: $surfaceHover; }
QPushButton#TagChip:checked {
    background: $primarySoft;
    color: $primary;
    border: 1px solid $primary;
}
QPushButton#IconButton {
    background: transparent;
    border: none;
    border-radius: 9px;
    padding: 6px 9px;
    font-size: 12pt;
}
QPushButton#IconButton:hover { background: $surfaceHover; }

QLineEdit, QDateTimeEdit, QSpinBox, QComboBox {
    background: $input;
    border: 1px solid $border;
    border-radius: 10px;
    padding: 7px 10px;
    selection-background-color: $primary;
    selection-color: #ffffff;
}
QLineEdit:focus, QDateTimeEdit:focus, QSpinBox:focus, QComboBox:focus {
    border: 1px solid $primary;
}
QSpinBox::up-button, QDateEdit::up-button {
    subcontrol-origin: border;
    subcontrol-position: top right;
    width: 18px;
    border: none;
    border-left: 1px solid $border;
    background: transparent;
    border-top-right-radius: 9px;
}
QSpinBox::down-button, QDateEdit::down-button {
    subcontrol-origin: border;
    subcontrol-position: bottom right;
    width: 18px;
    border: none;
    border-left: 1px solid $border;
    background: transparent;
    border-bottom-right-radius: 9px;
}
QSpinBox::up-arrow { image: url("$arrow_up"); width: 9px; height: 9px; }
QSpinBox::down-arrow { image: url("$arrow_down"); width: 9px; height: 9px; }
QDateTimeEdit::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: top right;
    width: 22px;
    border: none;
    background: transparent;
}
QDateTimeEdit::down-arrow { image: url("$arrow_down"); width: 10px; height: 10px; }

QLineEdit#TitleEdit {
    background: transparent;
    border: none;
    border-radius: 0;
    padding: 2px 0;
    font-size: 20px;
    font-weight: 700;
}
QLineEdit#SearchEdit {
    padding: 9px 14px;
    border-radius: 12px;
    font-size: 10.5pt;
}

QTextEdit {
    background: $input;
    border: 1px solid $border;
    border-radius: 12px;
    padding: 10px;
    font-size: 10.5pt;
    selection-background-color: $primary;
    selection-color: #ffffff;
}
QTextEdit:focus { border: 1px solid $primary; }

QFrame#Card {
    background: $surface;
    border: 1px solid $border;
    border-radius: 14px;
}
QFrame#Card:hover { background: $surfaceHover; border: 1px solid $primary; }
QFrame#Card[selected="true"] {
    background: $primarySoft;
    border: 2px solid $primary;
}
QFrame#Card[selected="true"] QLabel { color: $text; }
QFrame#Card[selected="true"] QCheckBox::indicator {
    background: transparent;
    border: 2px solid $primary;
}
QFrame#Card[selected="true"] QCheckBox::indicator:checked {
    background: $success;
    border: 2px solid $success;
}

QCheckBox { spacing: 8px; }
QCheckBox::indicator {
    width: 18px; height: 18px;
    border-radius: 5px;
    border: 2px solid $border;
    background: $input;
}
QCheckBox::indicator:hover { border: 2px solid $primary; }
QCheckBox::indicator:checked {
    background: $success;
    border: 2px solid $success;
    image: url("$check_icon");
}
QCheckBox#PrioCheck::indicator:checked {
    background: $primary;
    border: 2px solid $primary;
}

QScrollArea { border: none; background: transparent; }
QScrollArea > QWidget > QWidget { background: transparent; }
QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 2px;
}
QScrollBar::handle:vertical {
    background: $border;
    border-radius: 4px;
    min-height: 30px;
}
QScrollBar::handle:vertical:hover { background: $muted; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { height: 0; }

QListWidget#Attachments {
    background: $input;
    border: 1px dashed $border;
    border-radius: 12px;
    padding: 6px;
    outline: none;
}
QListWidget#Attachments::item {
    border-radius: 10px;
    padding: 4px;
    color: $muted;
}
QListWidget#Attachments::item:hover { background: $surfaceHover; color: $text; }
QListWidget#Attachments::item:selected {
    background: $primarySoft;
    color: $primary;
}
QListWidget#Tags {
    background: transparent;
    border: none;
    outline: none;
}
QListWidget#Tags::item { padding: 5px 8px; border-radius: 8px; color: $muted; }
QListWidget#Tags::item:hover { background: $surfaceHover; }
QListWidget#Tags::item:selected { background: $primarySoft; color: $primary; }

QMenuBar { background: $surface; color: $text; padding: 2px 6px; }
QMenuBar::item { padding: 6px 10px; border-radius: 6px; background: transparent; }
QMenuBar::item:selected { background: $surfaceHover; }
QMenu { background: $surface; border: 1px solid $border; border-radius: 10px; padding: 6px; }
QMenu::item { padding: 7px 26px; border-radius: 7px; }
QMenu::item:selected { background: $primarySoft; color: $primary; }
QMenu::separator { height: 1px; background: $border; margin: 6px 10px; }

QProgressBar {
    background: $surfaceAlt;
    border: none;
    border-radius: 4px;
    height: 8px;
    text-align: center;
    color: transparent;
}
QProgressBar::chunk { background: $success; border-radius: 4px; }

QSplitter::handle { background: transparent; width: 2px; }

QFrame#Toast {
    background: $toastBg;
    border-radius: 14px;
}
QLabel#ToastTitle { color: $toastText; font-weight: 700; font-size: 11pt; }
QLabel#ToastBody { color: $toastMuted; font-size: 10pt; }
QFrame#Toast QLabel#ToastIcon { color: $toastText; font-size: 16pt; }

QDialog { background: $surface; }
QFrame#EditorEmpty { background: $surface; }
QLabel#EmptyTitle { font-size: 18px; font-weight: 700; color: $text; }
QLabel#EmptyHint { font-size: 10.5pt; color: $muted; }

QStatusBar {
    background: $surface;
    color: $muted;
    border-top: 1px solid $border;
}
QStatusBar::item { border: none; }
"""
)


def build_qss(theme_name: str) -> str:
    p = dict(palette(theme_name))
    p["check_icon"] = (ASSETS / "check.svg").as_posix()
    p["arrow_up"] = (ASSETS / "arrow-up.svg").as_posix()
    p["arrow_down"] = (ASSETS / "arrow-down.svg").as_posix()
    p["version"] = __version__
    return QSS.substitute(p)


def chip_style(color: str, background: str, *, radius: int = 8, pad: str = "2px 8px") -> str:
    return (
        f"background: {background}; color: {color}; border-radius: {radius}px;"
        f" padding: {pad}; font-size: 9pt;"
    )

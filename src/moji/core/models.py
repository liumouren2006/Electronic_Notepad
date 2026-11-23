"""数据模型。

EventListener 的全部字段都会落盘到 UTF-8 文本文件（JSON），
附件则按事件 id 分目录存放，数据库中只保存相对路径。
"""

from __future__ import annotations

import os
import uuid
from dataclasses import dataclass, field
from datetime import date, datetime, timedelta
from typing import Any, Optional

ISO_FMT = "%Y-%m-%dT%H:%M:%S"


def now() -> datetime:
    return datetime.now().replace(microsecond=0)


def parse_datetime(value: Any) -> Optional[datetime]:
    """尽力把用户/文件里的各种时间写法解析成 datetime；失败返回 None。

    支持：datetime、date、ISO 字符串（含空格分隔、带时区）、
    ``YYYY-MM-DD``、``MM-DD``（按今年补全）、``HH:MM``（今天）。
    """
    if value is None or value == "":
        return None
    if isinstance(value, datetime):
        return value.replace(microsecond=0, tzinfo=None)
    if isinstance(value, date):
        return datetime(value.year, value.month, value.day)
    text = str(value).strip().replace("/", "-")
    if not text:
        return None
    try:
        dt = datetime.fromisoformat(text)
        return dt.replace(microsecond=0, tzinfo=None)
    except ValueError:
        pass
    fmts = [
        "%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M", "%Y-%m-%d",
        "%Y-%m-%dT%H:%M", "%Y%m%d", "%m-%d %H:%M", "%m-%d", "%H:%M",
    ]
    for fmt in fmts:
        try:
            dt = datetime.strptime(text, fmt)
        except ValueError:
            continue
        if fmt in ("%m-%d", "%m-%d %H:%M"):
            dt = dt.replace(year=now().year)
        elif fmt == "%H:%M":
            today = now()
            dt = today.replace(hour=dt.hour, minute=dt.minute, second=0)
        return dt
    return None


def fmt_dt(dt: Optional[datetime]) -> str:
    return dt.strftime(ISO_FMT) if dt else ""


def humanize_due(due: Optional[datetime], *, at: Optional[datetime] = None) -> str:
    """把截止时间变成人话：今天 14:30 / 明天 09:00 / 已逾期 2 小时 等。"""
    if not due:
        return "无期限"
    at = at or now()
    delta = due - at
    seconds = delta.total_seconds()
    same_day = due.date() == at.date()
    if seconds < 0:
        overdue = -seconds
        if overdue < 3600:
            return f"已逾期 {int(overdue // 60)} 分钟"
        if overdue < 86400:
            return f"已逾期 {int(overdue // 3600)} 小时"
        return f"已逾期 {int(overdue // 86400)} 天"
    if seconds < 3600:
        return f"{max(1, int(seconds // 60))} 分钟后"
    if same_day:
        return f"今天 {due:%H:%M}"
    if due.date() == at.date() + timedelta(days=1):
        return f"明天 {due:%H:%M}"
    if seconds < 7 * 86400:
        week = "一二三四五六日"[due.weekday()]
        return f"周{week} {due:%H:%M}"
    return f"{due:%Y-%m-%d %H:%M}"


@dataclass
class Attachment:
    """一个附件（图片/音频/任意文件）。path 为相对于数据目录的路径。"""

    path: str
    name: str = ""
    mime: str = ""
    size: int = 0
    added_at: Optional[datetime] = None

    def __post_init__(self) -> None:
        if not self.name:
            self.name = os.path.basename(self.path)
        if self.added_at is None:
            self.added_at = now()
        if not self.mime:
            self.mime = guess_mime(self.name)

    @property
    def is_image(self) -> bool:
        return self.mime.startswith("image/")

    @property
    def is_audio(self) -> bool:
        return self.mime.startswith("audio/")

    def to_dict(self) -> dict:
        return {
            "path": self.path,
            "name": self.name,
            "mime": self.mime,
            "size": int(self.size),
            "added_at": fmt_dt(self.added_at),
        }

    @classmethod
    def from_dict(cls, data: dict) -> "Attachment":
        path = str(data.get("path") or "")
        return cls(
            path=path,
            name=str(data.get("name") or os.path.basename(path)),
            mime=str(data.get("mime") or ""),
            size=int(data.get("size") or 0),
            added_at=parse_datetime(data.get("added_at")),
        )


def guess_mime(name: str) -> str:
    import mimetypes

    mime, _ = mimetypes.guess_type(name)
    if mime:
        return mime
    ext = os.path.splitext(name)[1].lower()
    extra = {
        ".md": "text/markdown", ".txt": "text/plain", ".wav": "audio/wav",
        ".mp3": "audio/mpeg", ".m4a": "audio/mp4", ".ogg": "audio/ogg",
        ".png": "image/png", ".jpg": "image/jpeg", ".jpeg": "image/jpeg",
        ".gif": "image/gif", ".webp": "image/webp", ".svg": "image/svg+xml",
    }
    return extra.get(ext, "application/octet-stream")


@dataclass
class Event:
    """一条待办/记事。

    priority 是 1 起始的位置序号，由 EventList 统一维护：
    在位置 k 插入新事件时，原来 k 及之后的事件整体后移一位。
    """

    title: str
    id: str = ""
    note: str = ""
    due: Optional[datetime] = None
    priority: int = 1
    done: bool = False
    done_at: Optional[datetime] = None
    tags: list[str] = field(default_factory=list)
    attachments: list[Attachment] = field(default_factory=list)
    created_at: Optional[datetime] = None
    updated_at: Optional[datetime] = None
    remind_before: int = 0          # 提前多少分钟提醒，0 表示到点提醒
    last_reminded_at: Optional[datetime] = None
    color: str = ""                 # GUI 便签颜色，留空用主题色

    def __post_init__(self) -> None:
        if not self.id:
            self.id = uuid.uuid4().hex[:8]
        if self.created_at is None:
            self.created_at = now()
        if self.updated_at is None:
            self.updated_at = self.created_at
        if self.done and self.done_at is None:
            self.done_at = now()
        if not self.done:
            self.done_at = None
        self.tags = [t for t in (str(x).strip() for x in self.tags) if t]
        self.priority = max(1, int(self.priority or 1))
        self.remind_before = max(0, int(self.remind_before or 0))

    # ---------- 派生属性 ----------
    @property
    def is_overdue(self) -> bool:
        return bool(self.due and not self.done and self.due < now())

    def is_due_on(self, day: date) -> bool:
        return bool(self.due and self.due.date() == day)

    def is_due_today(self) -> bool:
        return self.is_due_on(now().date())

    @property
    def due_today(self) -> bool:
        return self.is_due_today()

    def remind_at(self) -> Optional[datetime]:
        if not self.due:
            return None
        return self.due - timedelta(minutes=self.remind_before)

    def needs_reminder(self, at: Optional[datetime] = None) -> bool:
        """是否该提醒：到达提醒点、未完成、且本轮还没提醒过。"""
        at = at or now()
        if self.done or not self.due:
            return False
        moment = self.remind_at()
        if moment is None or at < moment:
            return False
        if self.last_reminded_at and self.last_reminded_at >= moment:
            return False
        return True

    # ---------- 序列化 ----------
    def to_dict(self) -> dict:
        return {
            "id": self.id,
            "title": self.title,
            "note": self.note,
            "due": fmt_dt(self.due),
            "priority": self.priority,
            "done": bool(self.done),
            "done_at": fmt_dt(self.done_at),
            "tags": list(self.tags),
            "attachments": [a.to_dict() for a in self.attachments],
            "created_at": fmt_dt(self.created_at),
            "updated_at": fmt_dt(self.updated_at),
            "remind_before": int(self.remind_before),
            "last_reminded_at": fmt_dt(self.last_reminded_at),
            "color": self.color,
        }

    @classmethod
    def from_dict(cls, data: dict) -> "Event":
        """容错解析：缺字段给默认值，坏附件跳过。"""
        if not isinstance(data, dict):
            raise ValueError("事件必须是对象")
        title = str(data.get("title") or "").strip()
        if not title:
            raise ValueError("事件缺少标题")
        attachments: list[Attachment] = []
        for item in data.get("attachments") or []:
            try:
                if isinstance(item, dict) and item.get("path"):
                    attachments.append(Attachment.from_dict(item))
            except Exception:
                continue
        tags_raw = data.get("tags") or []
        if isinstance(tags_raw, str):
            tags_raw = [t for t in tags_raw.replace("，", ",").split(",")]
        return cls(
            title=title,
            id=str(data.get("id") or ""),
            note=str(data.get("note") or ""),
            due=parse_datetime(data.get("due")),
            priority=int(data.get("priority") or 1),
            done=bool(data.get("done")),
            done_at=parse_datetime(data.get("done_at")),
            tags=[str(t) for t in tags_raw if str(t).strip()],
            attachments=attachments,
            created_at=parse_datetime(data.get("created_at")),
            updated_at=parse_datetime(data.get("updated_at")),
            remind_before=int(data.get("remind_before") or 0),
            last_reminded_at=parse_datetime(data.get("last_reminded_at")),
            color=str(data.get("color") or ""),
        )

    def clone(self) -> "Event":
        return Event.from_dict(self.to_dict())

    # ---------- 展示 ----------
    def display_title(self) -> str:
        return ("✓ " if self.done else "") + self.title

    def badge(self) -> str:
        if self.done:
            return "已完成"
        if self.is_overdue:
            return "逾期"
        if self.is_due_today():
            return "今天"
        return f"P{self.priority}"

    def __str__(self) -> str:
        status = "完成" if self.done else humanize_due(self.due)
        return f"P{self.priority:<2} {self.title}  [{status}]"

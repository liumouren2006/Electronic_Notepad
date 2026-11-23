"""业务仓储层：把链表、持久化与查询/撤销串起来。

所有写操作都会：
    1. 压入撤销栈；
    2. 更新 ``updated_at``；
    3. 重排优先级；
    4. 自动落盘（失败抛 RepoError，界面负责提示）。
"""

from __future__ import annotations

import os
from datetime import date, datetime, timedelta
from typing import Iterable, Optional

from .elist import EventList
from .models import Attachment, Event, now, parse_datetime
from .storage import Store, StoreError

MAX_UNDO = 60


class RepoError(Exception):
    """业务层错误，消息可直接展示给用户。"""


class Repository:
    def __init__(self, data_dir: Optional[str | os.PathLike] = None) -> None:
        self.store = Store(data_dir)
        self.events = EventList()
        self._undo: list[list[dict]] = []
        self._redo: list[list[dict]] = []
        self.load_warnings: list[str] = []
        self.reload()

    # ---------------- 读写 ----------------
    def reload(self) -> None:
        raw = self.store.load_events()
        events: list[Event] = []
        self.load_warnings = list(self.store.warnings)
        for item in raw:
            try:
                events.append(Event.from_dict(item))
            except (ValueError, TypeError, KeyError) as exc:
                self.load_warnings.append(f"跳过损坏记录：{exc}")
        events.sort(key=lambda e: (e.priority, e.created_at or now()))
        self.events = EventList(events)
        self.events.reindex()
        # 附件文件丢失时给出提示，但不删除记录
        for event in self.events:
            for att in event.attachments:
                if not self.store.attachment_exists(att):
                    self.load_warnings.append(f"「{event.title}」的附件已丢失：{att.name}")

    def save(self) -> None:
        try:
            self.store.save_events([e.to_dict() for e in self.events])
        except StoreError as exc:
            raise RepoError(str(exc)) from exc

    # ---------------- 撤销 / 重做 ----------------
    def _snapshot(self) -> list[dict]:
        return [e.to_dict() for e in self.events]

    def _push_undo(self) -> None:
        self._undo.append(self._snapshot())
        del self._undo[:-MAX_UNDO]
        self._redo.clear()

    def _restore(self, snap: list[dict]) -> None:
        self.events = EventList([Event.from_dict(d) for d in snap])
        self.events.reindex()

    def can_undo(self) -> bool:
        return bool(self._undo)

    def can_redo(self) -> bool:
        return bool(self._redo)

    def undo(self) -> bool:
        if not self._undo:
            return False
        self._redo.append(self._snapshot())
        self._restore(self._undo.pop())
        self.save()
        return True

    def redo(self) -> bool:
        if not self._redo:
            return False
        self._undo.append(self._snapshot())
        self._restore(self._redo.pop())
        self.save()
        return True

    # ---------------- 增 ----------------
    def add(
        self,
        title: str,
        note: str = "",
        due: datetime | str | None = None,
        tags: Iterable[str] | None = None,
        position: Optional[int] = None,
        remind_before: int = 0,
        color: str = "",
    ) -> Event:
        title = (title or "").strip()
        if not title:
            raise RepoError("标题不能为空")
        self._push_undo()
        event = Event(
            title=title,
            note=note or "",
            due=parse_datetime(due),
            tags=list(tags or []),
            remind_before=remind_before,
            color=color,
            created_at=now(),
            updated_at=now(),
        )
        if position is None:
            self.events.append(event)
        else:
            self.events.insert_at(position, event)
        self.events.reindex()
        self.save()
        return event

    def insert_after(self, anchor_id: str, title: str, **kwargs) -> Event:
        pos = self.events.index_of(anchor_id)
        if pos == -1:
            raise RepoError("找不到插入位置")
        return self.add(title, position=pos + 1, **kwargs)

    def duplicate(self, event_id: str) -> Event:
        src = self._require(event_id)
        self._push_undo()
        copy = src.clone()
        copy.id = Event(title="tmp").id
        copy.title = f"{src.title}（副本）"
        copy.created_at = copy.updated_at = now()
        copy.done = False
        copy.done_at = None
        copy.attachments = [Attachment.from_dict(a.to_dict()) for a in src.attachments]
        self.events.insert_at(self.events.index_of(event_id) + 1, copy)
        self.events.reindex()
        self.save()
        return copy

    # ---------------- 改 ----------------
    _EDITABLE = {"title", "note", "due", "tags", "remind_before", "color"}

    def update(self, event_id: str, **changes) -> Event:
        event = self._require(event_id)
        unknown = set(changes) - self._EDITABLE
        if unknown:
            raise RepoError(f"不支持修改字段：{', '.join(sorted(unknown))}")
        if "title" in changes:
            title = str(changes["title"]).strip()
            if not title:
                raise RepoError("标题不能为空")
            changes["title"] = title
        if "due" in changes:
            changes["due"] = parse_datetime(changes["due"])
        if "tags" in changes:
            raw = changes["tags"] or []
            if isinstance(raw, str):
                raw = raw.replace("，", ",").split(",")
            changes["tags"] = [str(t).strip() for t in raw if str(t).strip()]
        if "remind_before" in changes:
            try:
                changes["remind_before"] = max(0, int(changes["remind_before"]))
            except (TypeError, ValueError) as exc:
                raise RepoError("提醒提前量必须是整数分钟") from exc
        self._push_undo()
        for key, value in changes.items():
            setattr(event, key, value)
        event.updated_at = now()
        if "due" in changes:
            event.last_reminded_at = None
        self.events.reindex()
        self.save()
        return event

    def set_done(self, event_id: str, done: bool = True) -> Event:
        event = self._require(event_id)
        self._push_undo()
        event.done = bool(done)
        event.done_at = now() if done else None
        event.updated_at = now()
        self.save()
        return event

    def toggle_done(self, event_id: str) -> Event:
        event = self._require(event_id)
        return self.set_done(event_id, not event.done)

    def move_to(self, event_id: str, position: int) -> Event:
        event = self._require(event_id)
        self._push_undo()
        self.events.move_to(event_id, position)
        event.updated_at = now()
        self.save()
        return event

    def move_by(self, event_id: str, delta: int) -> Event:
        pos = self.events.index_of(event_id)
        if pos == -1:
            raise RepoError("事件不存在")
        return self.move_to(event_id, max(1, min(len(self.events), pos + delta)))

    def move_up(self, event_id: str) -> Event:
        return self.move_by(event_id, -1)

    def move_down(self, event_id: str) -> Event:
        return self.move_by(event_id, 1)

    # ---------------- 删 ----------------
    def delete(self, event_id: str) -> Event:
        event = self._require(event_id)
        self._push_undo()
        self.events.remove(event_id)
        self.events.reindex()
        # 附件文件先保留在磁盘上，撤销时可以原样恢复；清理用 gc_orphans()
        self.save()
        return event

    def clear_done(self) -> int:
        finished = [e.id for e in self.events if e.done]
        if not finished:
            return 0
        self._push_undo()
        for event_id in finished:
            self.events.remove(event_id)
        self.events.reindex()
        self.save()
        return len(finished)

    # ---------------- 查 ----------------
    def get(self, event_id: str) -> Optional[Event]:
        return self.events.find(event_id)

    def _require(self, event_id: str) -> Event:
        event = self.events.find(event_id)
        if event is None:
            raise RepoError(f"找不到事件：{event_id}")
        return event

    def all(self) -> list[Event]:
        return self.events.to_list()

    def pending(self) -> list[Event]:
        return [e for e in self.events if not e.done]

    def finished(self) -> list[Event]:
        return [e for e in self.events if e.done]

    def overdue(self, at: Optional[datetime] = None) -> list[Event]:
        at = at or now()
        return [e for e in self.events if e.due and not e.done and e.due < at]

    def due_on(self, day: date | str) -> list[Event]:
        day = _as_date(day)
        return [e for e in self.events if e.is_due_on(day)]

    def due_between(self, start: datetime, end: datetime) -> list[Event]:
        return [e for e in self.events if e.due and start <= e.due < end]

    def today(self) -> list[Event]:
        return self.due_on(now().date())

    def upcoming(self, days: int = 7) -> list[Event]:
        at = now()
        end = at + timedelta(days=days)
        return [e for e in self.events if e.due and not e.done and at <= e.due < end]

    def search(self, keyword: str) -> list[Event]:
        kw = (keyword or "").strip().lower()
        if not kw:
            return self.all()
        terms = kw.split()
        result = []
        for event in self.events:
            haystack = " ".join(
                [event.title, event.note, " ".join(event.tags)]
                + [a.name for a in event.attachments]
            ).lower()
            if all(term in haystack for term in terms):
                result.append(event)
        return result

    def tags(self) -> dict[str, int]:
        counts: dict[str, int] = {}
        for event in self.events:
            if event.done:
                continue
            for tag in event.tags:
                counts[tag] = counts.get(tag, 0) + 1
        return dict(sorted(counts.items(), key=lambda kv: (-kv[1], kv[0])))

    def stats(self) -> dict:
        total = len(self.events)
        done = len(self.finished())
        today = len(self.today())
        return {
            "total": total,
            "pending": total - done,
            "done": done,
            "today": today,
            "overdue": len(self.overdue()),
            "attachments": sum(len(e.attachments) for e in self.events),
            "rate": f"{(done / total * 100):.0f}%" if total else "0%",
        }

    def due_reminders(self, at: Optional[datetime] = None) -> list[Event]:
        at = at or now()
        return [e for e in self.events if e.needs_reminder(at)]

    def mark_reminded(self, events: Iterable[Event], at: Optional[datetime] = None) -> None:
        at = at or now()
        changed = False
        for event in events:
            event.last_reminded_at = at
            changed = True
        if changed:
            self.save()

    # ---------------- 附件 ----------------
    def add_attachment(self, event_id: str, src_path: str) -> Attachment:
        event = self._require(event_id)
        try:
            att = self.store.add_attachment(event_id, src_path)
        except StoreError as exc:
            raise RepoError(str(exc)) from exc
        self._push_undo()
        event.attachments.append(att)
        event.updated_at = now()
        self.save()
        return att

    def remove_attachment(self, event_id: str, index: int) -> Attachment:
        event = self._require(event_id)
        if not 0 <= index < len(event.attachments):
            raise RepoError("附件序号无效")
        self._push_undo()
        att = event.attachments.pop(index)
        # 不立即删文件，保证撤销后附件还能打开；孤儿文件交给 gc_orphans()
        event.updated_at = now()
        self.save()
        return att

    def attachment_path(self, att: Attachment):
        return self.store.resolve(att)

    def reorder_attachments(self, event_id: str, rel_paths: list[str]) -> Event:
        """按给定顺序重排附件（GUI 里拖动图标后调用）。"""
        event = self._require(event_id)
        by_path = {a.path: a for a in event.attachments}
        if set(rel_paths) != set(by_path):
            raise RepoError("附件顺序与现有附件不一致")
        self._push_undo()
        event.attachments = [by_path[p] for p in rel_paths]
        event.updated_at = now()
        self.save()
        return event

    def gc_orphans(self) -> int:
        """清理不再被任何事件引用的附件目录（删除/移除后遗留的文件）。"""
        keep = {e.id for e in self.events}
        removed = 0
        try:
            for folder in self.store.attachments_dir.iterdir():
                if folder.is_dir() and folder.name not in keep:
                    import shutil

                    shutil.rmtree(folder, ignore_errors=True)
                    removed += 1
        except OSError:
            pass
        return removed


def _as_date(value: date | str) -> date:
    if isinstance(value, date):
        return value
    parsed = parse_datetime(value)
    if not parsed:
        raise RepoError(f"无法识别的日期：{value}")
    return parsed.date()

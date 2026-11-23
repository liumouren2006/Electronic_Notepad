"""优先级链表。

作业要求里可以用“结构数组或简单链表”，这里刻意手写了一个单向链表，
并把它作为优先级的唯一权威来源：

    在位置 k 插入新事件 → 原 k 及之后的事件优先级整体 +1。

例如原来优先级为 1、2 的两件事，在中间插入新事件后，
新事件拿到 2，原来优先级 2 的事件自动变成 3。
"""

from __future__ import annotations

from typing import Iterator, Optional

from .models import Event


class _Node:
    __slots__ = ("event", "next")

    def __init__(self, event: Event) -> None:
        self.event = event
        self.next: Optional["_Node"] = None


class EventList:
    """按优先级排序的事件单向链表。"""

    def __init__(self, events: Optional[list[Event]] = None) -> None:
        self._head: Optional[_Node] = None
        self._size = 0
        if events:
            for event in events:
                self.append(event)
            self.reindex()

    # ---------------- 基础容器协议 ----------------
    def __len__(self) -> int:
        return self._size

    def __iter__(self) -> Iterator[Event]:
        node = self._head
        while node is not None:
            yield node.event
            node = node.next

    def __getitem__(self, index: int) -> Event:
        if index < 0:
            index += self._size
        if not 0 <= index < self._size:
            raise IndexError("索引越界")
        for i, event in enumerate(self):
            if i == index:
                return event
        raise IndexError("索引越界")

    def __contains__(self, event_id: str) -> bool:
        return self.find(event_id) is not None

    def to_list(self) -> list[Event]:
        return list(self)

    def ids(self) -> list[str]:
        return [e.id for e in self]

    # ---------------- 查找 ----------------
    def find(self, event_id: str) -> Optional[Event]:
        node = self._head
        while node is not None:
            if node.event.id == event_id:
                return node.event
            node = node.next
        return None

    def index_of(self, event_id: str) -> int:
        """返回 1 起始的优先级位置，找不到返回 -1。"""
        for i, event in enumerate(self, start=1):
            if event.id == event_id:
                return i
        return -1

    # ---------------- 插入 / 删除 ----------------
    def append(self, event: Event) -> Event:
        node = _Node(event)
        if self._head is None:
            self._head = node
        else:
            tail = self._head
            while tail.next is not None:
                tail = tail.next
            tail.next = node
        self._size += 1
        event.priority = self._size
        return event

    def insert_at(self, pos: int, event: Event) -> Event:
        """把事件插入到第 pos 位（1 起始，可等于 len+1）。

        原 pos 及之后的事件优先级顺延 +1。
        """
        pos = max(1, min(int(pos), self._size + 1))
        node = _Node(event)
        if pos == 1:
            node.next = self._head
            self._head = node
        else:
            prev = self._head
            for _ in range(pos - 2):
                prev = prev.next  # type: ignore[assignment]
            node.next = prev.next  # type: ignore[union-attr]
            prev.next = node  # type: ignore[union-attr]
        self._size += 1
        self.reindex()
        return event

    def insert_after(self, anchor_id: str, event: Event) -> Event:
        pos = self.index_of(anchor_id)
        if pos == -1:
            return self.append(event)
        return self.insert_at(pos + 1, event)

    def remove(self, event_id: str) -> Optional[Event]:
        prev: Optional[_Node] = None
        node = self._head
        while node is not None:
            if node.event.id == event_id:
                if prev is None:
                    self._head = node.next
                else:
                    prev.next = node.next
                self._size -= 1
                removed, node.next = node.event, None
                self.reindex()
                return removed
            prev, node = node, node.next
        return None

    def clear(self) -> None:
        self._head = None
        self._size = 0

    def extend(self, events: list[Event]) -> None:
        for event in events:
            self.append(event)
        self.reindex()

    def move_to(self, event_id: str, pos: int) -> bool:
        """把事件移动到新位置（1 起始）。"""
        event = self.find(event_id)
        if event is None:
            return False
        self.remove(event_id)
        self.insert_at(pos, event)
        return True

    def move_by(self, event_id: str, delta: int) -> bool:
        pos = self.index_of(event_id)
        if pos == -1:
            return False
        return self.move_to(event_id, pos + delta)

    def sort_by(self, key, reverse: bool = False) -> None:
        ordered = sorted(self.to_list(), key=key, reverse=reverse)
        self.clear()
        self.extend(ordered)

    # ---------------- 优先级编号 ----------------
    def reindex(self) -> None:
        for i, event in enumerate(self, start=1):
            event.priority = i

    def priorities(self) -> list[int]:
        return [e.priority for e in self]

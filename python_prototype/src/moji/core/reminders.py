"""提醒与蜂鸣。

作业里提到“利用 C 中的蜂鸣音提示”，这里用可移植的方式实现：
    - 所有平台：终端响铃 ``\\a``；
    - Windows：额外调用 ``winsound.MessageBeep``；
    - GUI 由 Qt 的 QApplication.beep() 负责。
"""

from __future__ import annotations

import sys
from datetime import datetime
from typing import Iterable, Optional

from .models import Event, humanize_due, now


def beep(times: int = 1) -> None:
    """蜂鸣提示，失败时静默。"""
    if sys.platform.startswith("win"):
        try:
            import winsound

            for _ in range(max(1, min(times, 5))):
                winsound.MessageBeep(winsound.MB_ICONEXCLAMATION)
        except Exception:
            pass
    try:
        sys.stdout.write("\a")
        sys.stdout.flush()
    except Exception:
        pass


def describe(event: Event, at: Optional[datetime] = None) -> str:
    at = at or now()
    due = humanize_due(event.due, at=at)
    return f"{event.title}（{due}）"


def reminder_text(events: Iterable[Event], at: Optional[datetime] = None) -> str:
    at = at or now()
    events = list(events)
    if not events:
        return ""
    lines = [describe(e, at) for e in events[:5]]
    if len(events) > 5:
        lines.append(f"……以及另外 {len(events) - 5} 件事")
    return "\n".join(lines)


def startup_summary(repo) -> str:
    """启动时的一句今日概览。"""
    today = repo.today()
    pending = [e for e in today if not e.done]
    overdue = repo.overdue()
    parts = [f"今天有 {len(today)} 项安排"]
    if pending:
        parts.append(f"其中 {len(pending)} 项未完成")
    if overdue:
        parts.append(f"{len(overdue)} 项已逾期")
    return "，".join(parts) + "。"

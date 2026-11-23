"""持久化层：UTF-8 文本文件 + 附件目录 + 自动备份 + 容错恢复。

数据目录优先级：
    1. 命令行 --data-dir
    2. 环境变量 MOJI_HOME
    3. 用户主目录下的 ~/.moji

目录结构：
    ~/.moji/
    ├── events.json        # 主数据（纯文本 JSON，可 diff、可同步）
    ├── events.json.bak    # 上一次成功保存的副本
    ├── config.json        # 主题、提醒开关等
    ├── backups/           # 最近 20 次自动备份
    └── attachments/
        └── <event-id>/    # 每个事件一个附件目录
"""

from __future__ import annotations

import json
import os
import re
import shutil
import sys
from datetime import datetime
from pathlib import Path
from typing import Any, Optional

from .models import Attachment, fmt_dt, guess_mime, now

APP_DIRNAME = ".moji"
EVENTS_FILE = "events.json"
CONFIG_FILE = "config.json"
DEFAULT_CONFIG: dict[str, Any] = {
    "version": 1,
    "theme": "light",
    "beep": True,
    "remind_interval_sec": 30,
    "keep_backups": 20,
    "window": {"w": 1180, "h": 760},
}


class StoreError(Exception):
    """存储层错误（权限、磁盘满、路径非法等）。"""


def resolve_data_dir(explicit: Optional[str | os.PathLike] = None) -> Path:
    if explicit:
        return Path(explicit).expanduser().resolve()
    env = os.environ.get("MOJI_HOME")
    if env:
        return Path(env).expanduser().resolve()
    return Path.home() / APP_DIRNAME


_SAFE_NAME = re.compile(r'[<>:"/\\|?*\x00-\x1f]')


def safe_filename(name: str, limit: int = 80) -> str:
    """把任意文件名清洗成 Windows/Linux 都安全的形式。"""
    base = _SAFE_NAME.sub("_", os.path.basename(name or "file")).strip(" .")
    if not base:
        base = "file"
    stem, ext = os.path.splitext(base)
    if len(stem) > limit:
        stem = stem[:limit]
    return stem + ext[:20]


def human_size(num: int) -> str:
    step = 1024.0
    size = float(num)
    for unit in ("B", "KB", "MB", "GB"):
        if size < step or unit == "GB":
            return f"{size:.0f} {unit}" if unit == "B" else f"{size:.1f} {unit}"
        size /= step
    return f"{num} B"


class Store:
    """负责把事件列表与附件安全地读写到磁盘。"""

    def __init__(self, data_dir: Optional[str | os.PathLike] = None) -> None:
        self.dir = resolve_data_dir(data_dir)
        self.events_path = self.dir / EVENTS_FILE
        self.backup_path = self.dir / (EVENTS_FILE + ".bak")
        self.config_path = self.dir / CONFIG_FILE
        self.attachments_dir = self.dir / "attachments"
        self.backups_dir = self.dir / "backups"
        self.warnings: list[str] = []
        self._ensure_dirs()

    # ---------------- 初始化 ----------------
    def _ensure_dirs(self) -> None:
        try:
            self.dir.mkdir(parents=True, exist_ok=True)
            self.attachments_dir.mkdir(exist_ok=True)
            self.backups_dir.mkdir(exist_ok=True)
        except OSError as exc:
            raise StoreError(f"无法创建数据目录 {self.dir}：{exc}") from exc

    # ---------------- 事件读写 ----------------
    def load_events(self) -> list[dict]:
        """读取事件原始字典列表；损坏时自动回退备份，绝不抛异常丢数据。"""
        self.warnings = []
        for path in (self.events_path, self.backup_path):
            if not path.exists():
                continue
            try:
                text = path.read_text(encoding="utf-8-sig")
                if not text.strip():
                    continue
                data = json.loads(text)
                if isinstance(data, dict):
                    data = data.get("events", [])
                if not isinstance(data, list):
                    raise ValueError("顶层不是数组")
                if path == self.backup_path:
                    self.warnings.append(f"主数据文件损坏或缺失，已从备份恢复：{path.name}")
                    self._restore_from_backup(text)
                return [item for item in data if isinstance(item, dict)]
            except (OSError, json.JSONDecodeError, ValueError) as exc:
                self.warnings.append(f"读取 {path.name} 失败：{exc}")
                if path == self.events_path:
                    self._quarantine(path)
        return []

    def _restore_from_backup(self, text: str) -> None:
        try:
            self.events_path.write_text(text, encoding="utf-8")
        except OSError:
            self.warnings.append("备份恢复写回失败，将以只读方式继续")

    def _quarantine(self, path: Path) -> None:
        try:
            stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
            target = self.dir / f"{path.name}.corrupt-{stamp}"
            path.replace(target)
            self.warnings.append(f"损坏文件已隔离为 {target.name}，可从备份手动恢复")
        except OSError:
            pass

    def save_events(self, events: list[dict]) -> None:
        payload = {
            "app": "moji-note",
            "version": 1,
            "updated_at": fmt_dt(now()),
            "count": len(events),
            "events": events,
        }
        text = json.dumps(payload, ensure_ascii=False, indent=2)
        self._atomic_write(self.events_path, text)
        try:
            self.backup_path.write_text(text, encoding="utf-8")
        except OSError:
            pass
        self._rotate_backup(text)
        # 保存后若发现附件索引与磁盘不符，不做破坏性清理，仅提示
        self._prune_empty_attachment_dirs()

    def _atomic_write(self, path: Path, text: str) -> None:
        tmp = path.with_suffix(path.suffix + ".tmp")
        try:
            with open(tmp, "w", encoding="utf-8", newline="\n") as fh:
                fh.write(text)
                fh.flush()
                os.fsync(fh.fileno())
            os.replace(tmp, path)
        except OSError as exc:
            try:
                if tmp.exists():
                    tmp.unlink()
            except OSError:
                pass
            raise StoreError(f"保存失败：{exc}") from exc

    def _rotate_backup(self, text: str) -> None:
        try:
            stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
            target = self.backups_dir / f"events-{stamp}.json"
            target.write_text(text, encoding="utf-8")
            keep = int(self.load_config().get("keep_backups", 20))
            backups = sorted(self.backups_dir.glob("events-*.json"))
            for old in backups[:-keep] if keep > 0 else backups:
                old.unlink(missing_ok=True)
        except OSError as exc:
            self.warnings.append(f"自动备份失败：{exc}")

    # ---------------- 命令行快照（跨进程撤销） ----------------
    def snapshot(self, tag: str = "undo", keep: int = 20) -> None:
        """把当前数据文件复制到 backups/<tag>-时间戳.json。

        TUI/GUI 的撤销栈在内存里，命令行每次都是新进程，
        所以写操作前额外落一份快照，`moji undo` 才能跨命令撤销。
        """
        if not self.events_path.exists():
            return
        try:
            stamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
            target = self.backups_dir / f"{tag}-{stamp}.json"
            target.write_text(
                self.events_path.read_text(encoding="utf-8-sig"), encoding="utf-8"
            )
            files = sorted(self.backups_dir.glob(f"{tag}-*.json"))
            for old in files[:-keep] if keep > 0 else files:
                old.unlink(missing_ok=True)
        except OSError as exc:
            self.warnings.append(f"创建撤销快照失败：{exc}")

    def restore_last(self, tag: str = "undo") -> bool:
        """恢复最近一份快照（恢复后删除该快照，可连续撤销）。"""
        files = sorted(self.backups_dir.glob(f"{tag}-*.json"))
        while files:
            latest = files.pop()
            try:
                text = latest.read_text(encoding="utf-8-sig")
                data = json.loads(text)
                if not isinstance(data, (dict, list)):
                    raise ValueError("快照格式不对")
            except (OSError, json.JSONDecodeError, ValueError):
                continue
            try:
                self._atomic_write(self.events_path, text)
                self.backup_path.write_text(text, encoding="utf-8")
            except OSError:
                return False
            latest.unlink(missing_ok=True)
            return True
        return False

    def has_snapshots(self, tag: str = "undo") -> bool:
        return any(self.backups_dir.glob(f"{tag}-*.json"))

    def clear_snapshots(self, tag: str) -> None:
        try:
            for path in self.backups_dir.glob(f"{tag}-*.json"):
                path.unlink(missing_ok=True)
        except OSError:
            pass

    # ---------------- 配置 ----------------
    def load_config(self) -> dict:
        cfg = dict(DEFAULT_CONFIG)
        try:
            if self.config_path.exists():
                data = json.loads(self.config_path.read_text(encoding="utf-8-sig"))
                if isinstance(data, dict):
                    cfg.update(data)
        except (OSError, json.JSONDecodeError) as exc:
            self.warnings.append(f"配置文件损坏，使用默认设置：{exc}")
        return cfg

    def save_config(self, config: dict) -> None:
        merged = dict(DEFAULT_CONFIG)
        merged.update(config or {})
        try:
            self._atomic_write(
                self.config_path,
                json.dumps(merged, ensure_ascii=False, indent=2),
            )
        except StoreError:
            pass

    # ---------------- 附件 ----------------
    def event_attachment_dir(self, event_id: str) -> Path:
        return self.attachments_dir / safe_filename(event_id)

    def add_attachment(self, event_id: str, src: str | os.PathLike) -> Attachment:
        src_path = Path(src).expanduser()
        if not src_path.is_file():
            raise StoreError(f"附件不存在或不是文件：{src}")
        target_dir = self.event_attachment_dir(event_id)
        target_dir.mkdir(parents=True, exist_ok=True)
        name = safe_filename(src_path.name)
        target = target_dir / name
        stem, ext = os.path.splitext(name)
        counter = 1
        while target.exists():
            target = target_dir / f"{stem}-{counter}{ext}"
            counter += 1
        try:
            shutil.copy2(src_path, target)
        except OSError as exc:
            raise StoreError(f"复制附件失败：{exc}") from exc
        rel = target.relative_to(self.dir).as_posix()
        return Attachment(
            path=rel,
            name=target.name,
            mime=guess_mime(target.name),
            size=target.stat().st_size,
            added_at=now(),
        )

    def resolve(self, att: Attachment | str) -> Path:
        rel = att.path if isinstance(att, Attachment) else att
        path = (self.dir / rel).resolve()
        try:
            path.relative_to(self.dir.resolve())
        except ValueError as exc:
            raise StoreError(f"附件路径越界：{rel}") from exc
        return path

    def attachment_exists(self, att: Attachment) -> bool:
        try:
            return self.resolve(att).is_file()
        except StoreError:
            return False

    def remove_attachments(self, event_id: str) -> None:
        folder = self.event_attachment_dir(event_id)
        try:
            if folder.exists():
                shutil.rmtree(folder)
        except OSError as exc:
            self.warnings.append(f"删除附件目录失败：{exc}")

    def _prune_empty_attachment_dirs(self) -> None:
        try:
            for folder in self.attachments_dir.iterdir():
                if folder.is_dir() and not any(folder.iterdir()):
                    folder.rmdir()
        except OSError:
            pass

    # ---------------- 数据转移 ----------------
    def info(self) -> dict:
        events_bytes = self.events_path.stat().st_size if self.events_path.exists() else 0
        n_att = sum(1 for _ in self.attachments_dir.rglob("*") if _.is_file())
        return {
            "data_dir": str(self.dir),
            "events_file": str(self.events_path),
            "events_size": events_bytes,
            "attachments": n_att,
            "cfg": self.load_config(),
        }


def warn(msg: str) -> None:
    print(msg, file=sys.stderr)

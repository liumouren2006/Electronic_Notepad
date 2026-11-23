"""导入 / 导出。

支持的格式：
    - .json      纯文本备份（不含附件文件，仅索引）
    - .mjnote    ZIP 打包（JSON + 附件），跨设备迁移用这个
    - .txt       todo.txt 生态格式（可被 Obsidian/TodoTxt 等工具读取）
    - .md        Markdown，便于打印、贴进博客
"""

from __future__ import annotations

import json
import re
import zipfile
from datetime import datetime
from pathlib import Path
from typing import Iterable

from .models import Event, fmt_dt, now, parse_datetime
from .repository import Repository, RepoError

BUNDLE_SUFFIX = ".mjnote"


# ---------------------------------------------------------------- JSON
def export_json(repo: Repository, path: str | Path) -> Path:
    path = Path(path)
    payload = {
        "app": "moji-note",
        "version": 1,
        "exported_at": fmt_dt(now()),
        "count": len(repo.events),
        "events": [e.to_dict() for e in repo.events],
    }
    try:
        path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    except OSError as exc:
        raise RepoError(f"导出失败：{exc}") from exc
    return path


def export_bundle(repo: Repository, path: str | Path) -> tuple[Path, int]:
    """打包 JSON + 附件，返回 (路径, 事件数)。"""
    path = Path(path)
    if path.suffix.lower() != BUNDLE_SUFFIX:
        path = path.with_suffix(BUNDLE_SUFFIX)
    events = []
    files: list[tuple[Path, str]] = []
    try:
        with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as zf:
            for event in repo.events:
                data = event.to_dict()
                for i, att in enumerate(event.attachments):
                    try:
                        src = repo.store.resolve(att)
                    except Exception:
                        continue
                    if not src.is_file():
                        continue
                    member = f"files/{event.id}/{i}_{Path(att.name).name}"
                    files.append((src, member))
                    data["attachments"][i]["src"] = member
                events.append(data)
            manifest = {
                "app": "moji-note",
                "version": 2,
                "exported_at": fmt_dt(now()),
                "count": len(events),
                "events": events,
            }
            zf.writestr("manifest.json", json.dumps(manifest, ensure_ascii=False, indent=2))
            for src, member in files:
                zf.write(src, member)
    except (OSError, zipfile.BadZipFile) as exc:
        raise RepoError(f"打包失败：{exc}") from exc
    return path, len(events)


def import_json(repo: Repository, path: str | Path, *, merge: bool = True) -> dict:
    path = Path(path)
    if path.suffix.lower() == BUNDLE_SUFFIX:
        return import_bundle(repo, path, merge=merge)
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as exc:
        raise RepoError(f"读取文件失败：{exc}") from exc
    if isinstance(data, dict):
        data = data.get("events", [])
    if not isinstance(data, list):
        raise RepoError("文件格式不正确：期望事件数组")
    return _merge_events(repo, data, {}, merge=merge, source=path.name)


def import_bundle(repo: Repository, path: str | Path, *, merge: bool = True) -> dict:
    path = Path(path)
    try:
        with zipfile.ZipFile(path) as zf:
            manifest = json.loads(zf.read("manifest.json").decode("utf-8"))
            raw = manifest.get("events", [])
            if not isinstance(raw, list):
                raise RepoError("清单格式不正确")
            # 先把附件解到临时目录，再走统一合并流程
            import tempfile

            tmpdir = Path(tempfile.mkdtemp(prefix="moji-import-"))
            extracted: dict[str, Path] = {}
            for name in zf.namelist():
                if name.startswith("files/") and not name.endswith("/"):
                    target = tmpdir / name.replace("/", "_")
                    target.write_bytes(zf.read(name))
                    extracted[name] = target
    except (OSError, zipfile.BadZipFile, json.JSONDecodeError, KeyError) as exc:
        raise RepoError(f"读取打包文件失败：{exc}") from exc
    try:
        return _merge_events(repo, raw, extracted, merge=merge, source=path.name)
    finally:
        import shutil

        shutil.rmtree(tmpdir, ignore_errors=True)


def _merge_events(
    repo: Repository,
    raw: list[dict],
    extracted: dict[str, Path],
    *,
    merge: bool,
    source: str,
) -> dict:
    existing_ids = {e.id for e in repo.events}
    existing_fingerprints = {
        (e.title, e.created_at) for e in repo.events
    }
    if not merge:
        repo._push_undo()
        repo.events.clear()
    added = skipped = 0
    for item in raw:
        if isinstance(item, dict):
            # 打包文件里附件的 zip 成员名放在 "src"，这里改写成可查找的 path
            for att in item.get("attachments") or []:
                if isinstance(att, dict) and att.get("src"):
                    att["path"] = att["src"]
        try:
            event = Event.from_dict(item)
        except (ValueError, TypeError):
            skipped += 1
            continue
        fingerprint = (event.title, event.created_at)
        if merge and (event.id in existing_ids or fingerprint in existing_fingerprints):
            skipped += 1
            continue
        if event.id in existing_ids:
            event.id = Event(title="tmp").id
        repo.events.append(event)
        existing_ids.add(event.id)
        # 附件迁移
        for att in event.attachments:
            src = extracted.get(att.path)
            if src is None:
                continue
            try:
                new_att = repo.store.add_attachment(event.id, src)
                att.path, att.name, att.mime, att.size = (
                    new_att.path, new_att.name, new_att.mime, new_att.size,
                )
            except Exception:
                pass
        added += 1
    repo.events.reindex()
    repo.save()
    return {"added": added, "skipped": skipped, "source": source}


# ---------------------------------------------------------------- todo.txt
_PRIORITY_LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"


def _todo_priority(event: Event) -> str:
    idx = min(max(event.priority - 1, 0), len(_PRIORITY_LETTERS) - 1)
    return f"({_PRIORITY_LETTERS[idx]}) "


def export_todotxt(repo: Repository, path: str | Path) -> Path:
    path = Path(path)
    lines = []
    for event in repo.events:
        parts = []
        if event.done:
            parts.append("x")
            parts.append((event.done_at or now()).strftime("%Y-%m-%d"))
        parts.append(_todo_priority(event).strip())
        if event.created_at:
            parts.append(event.created_at.strftime("%Y-%m-%d"))
        parts.append(event.title.replace("\n", " "))
        if event.due:
            parts.append("due:" + event.due.strftime("%Y-%m-%dT%H:%M"))
        parts.extend("+" + tag.replace(" ", "_") for tag in event.tags)
        lines.append(" ".join(p for p in parts if p))
    try:
        path.write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")
    except OSError as exc:
        raise RepoError(f"导出失败：{exc}") from exc
    return path


def import_todotxt(repo: Repository, path: str | Path, *, merge: bool = True) -> dict:
    path = Path(path)
    try:
        lines = path.read_text(encoding="utf-8-sig").splitlines()
    except OSError as exc:
        raise RepoError(f"读取文件失败：{exc}") from exc
    raw: list[dict] = []
    for line in lines:
        line = line.strip()
        if not line:
            continue
        done = False
        if line.startswith("x "):
            done = True
            line = line[2:].strip()
        priority = 1
        tokens = line.split()
        due = None
        tags = []
        title_tokens = []
        created = None
        for token in tokens:
            low = token.lower()
            if re.fullmatch(r"\([A-Za-z]\)", token):
                letter = token[1].upper()
                if letter in _PRIORITY_LETTERS:
                    priority = _PRIORITY_LETTERS.index(letter) + 1
            elif low.startswith("due:"):
                due = parse_datetime(token[4:])
            elif low.startswith("t:") and len(token) > 2:
                continue
            elif token.startswith("+"):
                tags.append(token[1:].replace("_", " "))
            elif _looks_like_date(token):
                created = parse_datetime(token)
            else:
                title_tokens.append(token)
        title = " ".join(title_tokens).strip()
        if not title:
            continue
        raw.append({
            "title": title,
            "due": fmt_dt(due),
            "priority": priority,
            "done": done,
            "tags": tags,
            "created_at": fmt_dt(created or now()),
        })
    return _merge_events(repo, raw, {}, merge=merge, source=path.name)


def _looks_like_date(token: str) -> bool:
    try:
        datetime.strptime(token, "%Y-%m-%d")
        return True
    except ValueError:
        return False


# ---------------------------------------------------------------- Markdown
def export_markdown(repo: Repository, path: str | Path) -> Path:
    path = Path(path)
    out = ["# 墨记 · 记事本导出", "", f"> 导出时间：{now():%Y-%m-%d %H:%M}　共 {len(repo.events)} 条", ""]
    groups: list[tuple[str, Iterable[Event]]] = [
        ("进行中", repo.pending()),
        ("已完成", repo.finished()),
    ]
    for title, events in groups:
        events = list(events)
        out.append(f"## {title}（{len(events)}）")
        out.append("")
        if not events:
            out.append("_暂无_")
            out.append("")
            continue
        for event in events:
            box = "x" if event.done else " "
            out.append(f"### {event.priority}. [{box}] {event.title}")
            if event.due:
                out.append(f"- 截止：{event.due:%Y-%m-%d %H:%M}")
            else:
                out.append("- 截止：无期限")
            if event.tags:
                out.append("- 标签：" + " ".join(f"`#{t}`" for t in event.tags))
            if event.attachments:
                out.append(
                    "- 附件：" + "、".join(f"{a.name}（{a.mime}）" for a in event.attachments)
                )
            if event.note:
                out.append("")
                for line in event.note.splitlines():
                    out.append("> " + line)
            out.append("")
    try:
        path.write_text("\n".join(out), encoding="utf-8")
    except OSError as exc:
        raise RepoError(f"导出失败：{exc}") from exc
    return path


def export_auto(repo: Repository, path: str | Path) -> Path:
    """按扩展名选择格式。"""
    suffix = Path(path).suffix.lower()
    if suffix == BUNDLE_SUFFIX:
        return export_bundle(repo, path)[0]
    if suffix in (".txt",):
        return export_todotxt(repo, path)
    if suffix in (".md", ".markdown"):
        return export_markdown(repo, path)
    return export_json(repo, path)


def import_auto(repo: Repository, path: str | Path, *, merge: bool = True) -> dict:
    suffix = Path(path).suffix.lower()
    if suffix == BUNDLE_SUFFIX:
        return import_bundle(repo, path, merge=merge)
    if suffix == ".txt":
        return import_todotxt(repo, path, merge=merge)
    if suffix in (".md", ".markdown"):
        raise RepoError("暂不支持 Markdown 反向导入，请使用 .json / .mjnote / .txt")
    return import_json(repo, path, merge=merge)

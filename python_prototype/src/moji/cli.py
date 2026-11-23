"""命令行入口。

    moji                # 默认进入 TUI
    moji gui            # MD 风格图形界面
    moji add "买牛奶" -u 2026-11-24T18:00 -t 生活
    moji list --today
    moji done 3         # 按优先级序号完成第 3 条
"""

from __future__ import annotations

import argparse
import os
import sys
import unicodedata

from . import __app_name__, __version__
from .core import Repository, RepoError
from .core import importer
from .core.models import humanize_due

# Windows 控制台开启 ANSI 颜色
if sys.platform.startswith("win"):
    os.system("")

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")  # type: ignore[attr-defined]
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")  # type: ignore[attr-defined]
except Exception:
    pass


class C:
    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"
    GRAY = "\033[90m"


def use_color() -> bool:
    return sys.stdout.isatty() and not os.environ.get("NO_COLOR")


def paint(text: str, color: str) -> str:
    return f"{color}{text}{C.RESET}" if use_color() else text


def width(text: str) -> int:
    return sum(
        2 if unicodedata.east_asian_width(ch) in ("W", "F") else 1 for ch in text
    )


def pad(text: str, width_: int) -> str:
    return text + " " * max(0, width_ - width(text))


def short_due(event) -> str:
    if not event.due:
        return "—"
    return humanize_due(event.due)


def due_color(event) -> str:
    if event.done:
        return C.GRAY
    if event.is_overdue:
        return C.RED
    if event.is_due_today():
        return C.YELLOW
    return C.CYAN


# ---------------------------------------------------------------- 展示
def print_events(events, *, title: str = "") -> None:
    if title:
        print("\n" + paint(title, C.BOLD + C.MAGENTA))
    if not events:
        print(paint("  （暂无记录）", C.GRAY))
        return
    header = f"{pad('优先级', 8)}{pad('状态', 8)}{pad('标题', 34)}{pad('截止', 22)}标签"
    print(paint(header, C.GRAY))
    print(paint("-" * min(100, width(header)), C.GRAY))
    for event in events:
        mark = "✓" if event.done else "○"
        status = paint(mark, C.GREEN if event.done else C.GRAY)
        title_text = event.title
        if width(title_text) > 30:
            title_text = title_text[: 14] + "…" + title_text[-8:]
        if event.done:
            title_text = paint(title_text, C.GRAY)
        due_text = paint(pad(short_due(event), 22), due_color(event))
        tags = paint(" ".join("#" + t for t in event.tags), C.BLUE)
        print(
            f"{pad('P' + str(event.priority), 8)}{pad(status, 8)}"
            f"{pad(title_text, 34)}{due_text}{tags}"
        )
    print(paint(f"  共 {len(events)} 条", C.GRAY))


def resolve_id(repo: Repository, token: str) -> str:
    token = token.strip().lstrip("#")
    if not token:
        raise RepoError("请给出事件编号或 id")
    if token.isdigit():
        idx = int(token)
        if 1 <= idx <= len(repo.events):
            return repo.events[idx - 1].id
        raise RepoError(f"没有第 {idx} 条记录")
    matches = [e.id for e in repo.events if e.id.startswith(token)]
    if len(matches) == 1:
        return matches[0]
    if not matches:
        raise RepoError(f"找不到编号为 {token} 的记录")
    raise RepoError(f"编号前缀 {token} 不唯一：{', '.join(matches)}")


# ---------------------------------------------------------------- 子命令
def cmd_add(repo: Repository, args) -> int:
    title = " ".join(args.title).strip()
    event = repo.add(
        title,
        note=args.note or "",
        due=args.due,
        tags=args.tag,
        position=args.pos,
        remind_before=args.remind,
    )
    print(paint(f"已添加 P{event.priority}：{event.title}  [{humanize_due(event.due)}]", C.GREEN))
    return 0


def cmd_list(repo: Repository, args) -> int:
    if args.date:
        events, title = repo.due_on(args.date), f"{args.date} 的安排"
    elif args.today:
        events, title = repo.today(), "今日安排"
    elif args.overdue:
        events, title = repo.overdue(), "已逾期"
    elif args.pending:
        events, title = repo.pending(), "未完成"
    elif args.done:
        events, title = repo.finished(), "已完成"
    elif args.tag:
        events = [e for e in repo.all() if args.tag in e.tags]
        title = f"标签 #{args.tag}"
    elif args.search:
        events, title = repo.search(args.search), f"搜索「{args.search}」"
    else:
        events, title = repo.all(), "全部安排"
    if args.limit:
        events = events[: args.limit]
    print_events(events, title=title)
    return 0


def cmd_show(repo: Repository, args) -> int:
    event = repo.get(resolve_id(repo, args.id))
    if not event:
        raise RepoError("记录不存在")
    print(paint(f"P{event.priority}  {event.title}", C.BOLD + C.MAGENTA))
    print(f"状态：{'已完成' if event.done else '进行中'}")
    print(f"截止：{humanize_due(event.due)}  ({event.due:%Y-%m-%d %H:%M})" if event.due else "截止：无")
    print(f"标签：{' '.join(event.tags) or '—'}")
    print(f"创建：{event.created_at:%Y-%m-%d %H:%M}　更新：{event.updated_at:%Y-%m-%d %H:%M}")
    print(f"提醒：提前 {event.remind_before} 分钟")
    if event.attachments:
        print("附件：")
        for i, att in enumerate(event.attachments, 1):
            exists = "✓" if repo.store.attachment_exists(att) else "✗ 丢失"
            print(f"  {i}. {att.name}  [{att.mime}] {exists}")
    if event.note:
        print("\n" + event.note)
    return 0


def cmd_done(repo: Repository, args) -> int:
    event = repo.set_done(resolve_id(repo, args.id), not args.undo)
    print(paint(("已撤销完成：" if args.undo else "已完成：") + event.title, C.GREEN))
    return 0


def cmd_rm(repo: Repository, args) -> int:
    event = repo.delete(resolve_id(repo, args.id))
    print(paint(f"已删除：{event.title}", C.YELLOW))
    return 0


def cmd_search(repo: Repository, args) -> int:
    print_events(repo.search(" ".join(args.keyword)), title=f"搜索「{' '.join(args.keyword)}」")
    return 0


def cmd_today(repo: Repository, args) -> int:
    print_events(repo.today(), title="今日安排")
    return 0


def cmd_stats(repo: Repository, args) -> int:
    stats = repo.stats()
    print(paint("墨记 · 数据概览", C.BOLD + C.MAGENTA))
    print(f"  全部 {stats['total']} 条　待办 {stats['pending']}　已完成 {stats['done']}"
          f"　完成率 {stats['rate']}")
    print(f"  今天 {stats['today']}　逾期 {stats['overdue']}　附件 {stats['attachments']}")
    print(f"  数据目录：{repo.store.dir}")
    tags = repo.tags()
    if tags:
        print("  标签：" + "  ".join(f"#{t}×{n}" for t, n in tags.items()))
    return 0


def cmd_undo(repo: Repository, args) -> int:
    if args.redo:
        if repo.can_redo():
            repo.store.snapshot("undo")
            ok = repo.redo()
        elif repo.store.has_snapshots("redo"):
            repo.store.snapshot("undo")
            ok = repo.store.restore_last("redo")
            if ok:
                repo.reload()
        else:
            ok = False
        text = "已重做" if ok else "没有可重做的操作"
    else:
        if repo.can_undo():
            ok = repo.undo()
        elif repo.store.has_snapshots("undo"):
            repo.store.snapshot("redo")
            ok = repo.store.restore_last("undo")
            if ok:
                repo.reload()
        else:
            ok = False
        text = "已撤销" if ok else "没有可撤销的操作"
    print(paint(text, C.GREEN if ok else C.GRAY))
    return 0 if ok else 1


def cmd_export(repo: Repository, args) -> int:
    path = importer.export_auto(repo, args.path)
    print(paint(f"已导出到 {path}", C.GREEN))
    return 0


def cmd_import(repo: Repository, args) -> int:
    result = importer.import_auto(repo, args.path, merge=not args.replace)
    print(paint(f"导入完成：新增 {result['added']} 条，跳过 {result['skipped']} 条", C.GREEN))
    return 0


def cmd_doctor(repo: Repository, args) -> int:
    from . import __version__

    info = repo.store.info()
    print(paint(f"{__app_name__} v{__version__}", C.BOLD + C.MAGENTA))
    print(f"  Python：{sys.version.split()[0]}  ({sys.executable})")
    for mod, label in (("textual", "TUI textual"), ("PySide6", "GUI PySide6")):
        try:
            module = __import__(mod)
            ver = getattr(module, "__version__", "")
            print(f"  {label}：{ver or '已安装'}")
        except ImportError:
            print(f"  {label}：" + paint("未安装", C.YELLOW))
    print(f"  数据目录：{info['data_dir']}")
    print(f"  事件文件：{info['events_file']}（{info['events_size']} 字节）")
    print(f"  附件数量：{info['attachments']}")
    stats = repo.stats()
    print(f"  事件统计：{stats}")
    warns = (repo.load_warnings or []) + repo.store.warnings
    if warns:
        print(paint("  警告：", C.YELLOW))
        for msg in warns:
            print("   - " + msg)
    else:
        print(paint("  数据检查：通过", C.GREEN))
    # 写权限测试
    probe = repo.store.dir / ".write-test"
    try:
        probe.write_text("ok", encoding="utf-8")
        probe.unlink()
        print(paint("  写权限：正常", C.GREEN))
    except OSError as exc:
        print(paint(f"  写权限：异常 {exc}", C.RED))
    return 0


def cmd_seed_demo(repo: Repository, args) -> int:
    if repo.all() and not args.force:
        raise RepoError("当前数据目录已有记录；如仍要写入演示数据，请加 --force")
    from .demo import seed_demo

    count = seed_demo(repo)
    print(paint(f"已写入 {count} 条演示数据到 {repo.store.dir}", C.GREEN))
    return 0


# ---------------------------------------------------------------- 启动 UI
def cmd_tui(repo: Repository, args) -> int:
    try:
        from .tui.app import MojiTUI
    except ImportError as exc:
        print(paint(f"TUI 无法启动：{exc}\n请先执行：pip install textual", C.RED))
        return 2
    MojiTUI(repo).run()
    return 0


def cmd_gui(repo: Repository, args) -> int:
    try:
        from .gui.app import launch_gui
    except ImportError as exc:
        print(paint(f"GUI 无法启动：{exc}\n请先执行：pip install PySide6", C.RED))
        return 2
    return launch_gui(repo)


# ---------------------------------------------------------------- 解析
def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="moji",
        description=f"{__app_name__} —— 带优先级链表与 TUI/GUI 双界面的电子记事本",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""示例：
  moji                                  打开 TUI（默认）
  moji gui                              打开图形界面
  moji add 买牛奶 -u 2026-11-24T18:00 -t 生活
  moji list --today                     查看今日安排
  moji done 2                           完成第 2 条
  moji export ~/Desktop/backup.mjnote   导出（含附件）
""",
    )
    parser.add_argument("-V", "--version", action="version", version=f"{__app_name__} {__version__}")
    parser.add_argument("-d", "--data-dir", help="数据目录（默认 ~/.moji，可用 MOJI_HOME 覆盖）")
    sub = parser.add_subparsers(dest="command")

    p = sub.add_parser("tui", help="启动终端界面（默认）")
    p.set_defaults(func=cmd_tui)

    p = sub.add_parser("gui", help="启动图形界面")
    p.set_defaults(func=cmd_gui)

    p = sub.add_parser("add", help="新增一条记事")
    p.add_argument("title", nargs="+", help="标题")
    p.add_argument("-u", "--due", help="截止时间，如 2026-11-24T18:00 / 明天 18:00 暂不支持，请用完整日期")
    p.add_argument("-n", "--note", help="备注")
    p.add_argument("-t", "--tag", action="append", default=[], help="标签，可多次指定")
    p.add_argument("-p", "--pos", type=int, help="插入到第几位（原有事件优先级顺延）")
    p.add_argument("-r", "--remind", type=int, default=0, help="提前多少分钟提醒")
    p.set_defaults(func=cmd_add)

    p = sub.add_parser("list", help="列出记事")
    p.add_argument("--date", help="按日期筛选，如 2026-11-24")
    p.add_argument("--today", action="store_true", help="仅今日")
    p.add_argument("--overdue", action="store_true", help="仅逾期")
    p.add_argument("--pending", action="store_true", help="仅未完成")
    p.add_argument("--done", action="store_true", help="仅已完成")
    p.add_argument("--tag", help="按标签筛选")
    p.add_argument("--search", "-s", help="关键字搜索")
    p.add_argument("--limit", type=int, help="最多显示条数")
    p.set_defaults(func=cmd_list)

    p = sub.add_parser("show", help="查看一条记事的详情")
    p.add_argument("id", help="事件 id 前缀或优先级序号")
    p.set_defaults(func=cmd_show)

    p = sub.add_parser("done", help="标记完成 / 取消完成")
    p.add_argument("id")
    p.add_argument("--undo", action="store_true", help="取消完成")
    p.set_defaults(func=cmd_done)

    p = sub.add_parser("rm", help="删除记事")
    p.add_argument("id")
    p.set_defaults(func=cmd_rm)

    p = sub.add_parser("search", help="全文搜索")
    p.add_argument("keyword", nargs="+")
    p.set_defaults(func=cmd_search)

    p = sub.add_parser("today", help="今日安排")
    p.set_defaults(func=cmd_today)

    p = sub.add_parser("stats", help="统计信息")
    p.set_defaults(func=cmd_stats)

    p = sub.add_parser("undo", help="撤销上一步")
    p.add_argument("--redo", action="store_true")
    p.set_defaults(func=cmd_undo)

    p = sub.add_parser("export", help="导出 .json/.mjnote/.txt/.md")
    p.add_argument("path")
    p.set_defaults(func=cmd_export)

    p = sub.add_parser("import", help="导入 .json/.mjnote/.txt")
    p.add_argument("path")
    p.add_argument("--replace", action="store_true", help="清空现有数据后再导入")
    p.set_defaults(func=cmd_import)

    p = sub.add_parser("seed-demo", help="写入演示数据")
    p.add_argument("--force", action="store_true")
    p.set_defaults(func=cmd_seed_demo)

    p = sub.add_parser("doctor", help="环境与数据自检")
    p.set_defaults(func=cmd_doctor)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        repo = Repository(args.data_dir)
    except Exception as exc:
        print(paint(f"无法打开数据目录：{exc}", C.RED), file=sys.stderr)
        return 2
    for msg in repo.load_warnings:
        print(paint("提示：" + msg, C.YELLOW), file=sys.stderr)
    if not getattr(args, "command", None):
        if sys.stdin.isatty() and sys.stdout.isatty():
            return cmd_tui(repo, args)
        parser.print_help()
        return 0
    # 命令行写操作前落快照，让 `moji undo` 能跨进程撤销
    if getattr(args, "command", None) in {"add", "done", "rm", "import", "seed-demo"}:
        repo.store.snapshot("undo")
        repo.store.clear_snapshots("redo")
    try:
        return args.func(repo, args)
    except RepoError as exc:
        print(paint(f"操作失败：{exc}", C.RED), file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print(paint("\n已取消", C.GRAY))
        return 130


if __name__ == "__main__":
    raise SystemExit(main())

"""核心层单元测试：python -m unittest discover -s tests -v"""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from datetime import datetime, timedelta
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from moji.core import Event, EventList, Repository, RepoError, parse_datetime  # noqa: E402
from moji.core import importer  # noqa: E402
from moji.core.models import humanize_due  # noqa: E402
from moji.core.reminders import reminder_text  # noqa: E402
from moji.core.storage import StoreError  # noqa: E402


class TempRepo(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory(prefix="moji-test-")
        self.dir = Path(self._tmp.name)
        self.repo = Repository(self.dir)

    def tearDown(self) -> None:
        self._tmp.cleanup()


class TestPriorityList(unittest.TestCase):
    def test_insert_shifts_priorities(self):
        lst = EventList()
        a = Event("买菜")
        b = Event("写作业")
        lst.append(a)
        lst.append(b)
        self.assertEqual([a.priority, b.priority], [1, 2])

        c = Event("取快递")
        lst.insert_at(2, c)

        self.assertEqual([a.priority, c.priority, b.priority], [1, 2, 3])
        self.assertEqual(lst.ids(), [a.id, c.id, b.id])
        self.assertEqual(b.priority, 3)  # 作业原文的规则：原来 2 → 变 3

    def test_insert_head_and_tail(self):
        lst = EventList([Event("A"), Event("B")])
        head = Event("新头")
        lst.insert_at(1, head)
        self.assertEqual(lst.ids()[0], head.id)
        self.assertEqual([e.priority for e in lst], [1, 2, 3])
        tail = Event("新尾")
        lst.insert_at(99, tail)
        self.assertEqual(lst.ids()[-1], tail.id)

    def test_remove_and_move(self):
        lst = EventList([Event(f"任务{i}") for i in range(4)])
        victim = lst[1]
        lst.remove(victim.id)
        self.assertEqual([e.priority for e in lst], [1, 2, 3])
        target = lst[0]
        lst.move_to(target.id, 3)
        self.assertEqual(lst.ids()[2], target.id)
        self.assertEqual([e.priority for e in lst], [1, 2, 3])

    def test_insert_after(self):
        lst = EventList([Event("A"), Event("B")])
        anchor = lst[0]
        new = Event("A2")
        lst.insert_after(anchor.id, new)
        self.assertEqual([e.title for e in lst], ["A", "A2", "B"])


class TestRepository(TempRepo):
    def test_add_and_persist(self):
        e1 = self.repo.add("买牛奶", due="2026-11-24T18:00", tags=["生活"])
        e2 = self.repo.add("写报告", tags=["课程"])
        self.assertEqual((e1.priority, e2.priority), (1, 2))

        repo2 = Repository(self.dir)
        titles = [e.title for e in repo2.all()]
        self.assertEqual(titles, ["买牛奶", "写报告"])
        self.assertEqual(repo2.all()[0].due, datetime(2026, 11, 24, 18, 0))
        self.assertEqual(repo2.all()[0].tags, ["生活"])

    def test_insert_position(self):
        a = self.repo.add("优先级1")
        b = self.repo.add("优先级2")
        c = self.repo.add("插队", position=2)
        self.assertEqual([e.title for e in self.repo.all()], ["优先级1", "插队", "优先级2"])
        self.assertEqual(b.priority, 3)
        self.assertEqual(c.priority, 2)

    def test_insert_after(self):
        a = self.repo.add("A")
        self.repo.add("C")
        self.repo.insert_after(a.id, "B")
        self.assertEqual([e.title for e in self.repo.all()], ["A", "B", "C"])

    def test_update_and_validation(self):
        e = self.repo.add("旧标题", due="2026-11-25")
        self.repo.update(e.id, title="新标题", due="2026-11-26 09:30", tags="a, b，c")
        loaded = self.repo.get(e.id)
        self.assertEqual(loaded.title, "新标题")
        self.assertEqual(loaded.due, datetime(2026, 11, 26, 9, 30))
        self.assertEqual(loaded.tags, ["a", "b", "c"])
        with self.assertRaises(RepoError):
            self.repo.update(e.id, title="   ")
        with self.assertRaises(RepoError):
            self.repo.update(e.id, bogus=1)
        with self.assertRaises(RepoError):
            self.repo.add("")

    def test_delete_and_clear_done(self):
        e = self.repo.add("要删除")
        self.repo.add("保留")
        self.repo.delete(e.id)
        self.assertEqual([x.title for x in self.repo.all()], ["保留"])
        self.assertEqual(self.repo.all()[0].priority, 1)
        self.repo.set_done(self.repo.all()[0].id)
        self.assertEqual(self.repo.clear_done(), 1)
        self.assertEqual(len(self.repo.all()), 0)

    def test_undo_redo(self):
        e = self.repo.add("原始")
        self.repo.update(e.id, title="改过")
        self.assertTrue(self.repo.undo())
        self.assertEqual(self.repo.get(e.id).title, "原始")
        self.assertTrue(self.repo.redo())
        self.assertEqual(self.repo.get(e.id).title, "改过")
        Repository(self.dir)  # 撤销/重做均已落盘

    def test_queries(self):
        today = datetime.now().replace(hour=15, minute=0, second=0, microsecond=0)
        yesterday = today - timedelta(days=1)
        a = self.repo.add("今天的事", due=today, tags=["x"])
        b = self.repo.add("昨天逾期", due=yesterday, tags=["x", "y"])
        c = self.repo.add("没有期限")
        self.repo.set_done(c.id)
        self.assertEqual([e.id for e in self.repo.today()], [a.id])
        self.assertEqual([e.id for e in self.repo.overdue()], [b.id])
        self.assertEqual([e.id for e in self.repo.search("逾期")], [b.id])
        self.assertEqual([e.id for e in self.repo.search("y")], [b.id])
        self.assertIn("x", self.repo.tags())
        stats = self.repo.stats()
        self.assertEqual(stats["total"], 3)
        self.assertEqual(stats["done"], 1)

    def test_move_priority(self):
        a = self.repo.add("A")
        b = self.repo.add("B")
        c = self.repo.add("C")
        self.repo.move_up(c.id)
        self.assertEqual([e.title for e in self.repo.all()], ["A", "C", "B"])
        self.repo.move_to(b.id, 1)
        self.assertEqual([e.title for e in self.repo.all()], ["B", "A", "C"])
        self.repo.move_down(b.id)
        self.assertEqual([e.title for e in self.repo.all()], ["A", "B", "C"])

    def test_attachments(self):
        e = self.repo.add("带附件")
        src = self.dir / "素材.txt"
        src.write_text("附件内容", encoding="utf-8")
        att = self.repo.add_attachment(e.id, str(src))
        self.assertTrue(self.repo.store.attachment_exists(att))
        loaded = Repository(self.dir).get(e.id)
        self.assertEqual(len(loaded.attachments), 1)
        path = self.repo.attachment_path(loaded.attachments[0])
        self.assertEqual(path.read_text(encoding="utf-8"), "附件内容")
        # 删除事件后附件文件仍保留（撤销/恢复友好），gc 时才清理
        folder = self.repo.store.event_attachment_dir(e.id)
        self.repo.delete(e.id)
        self.assertTrue(folder.exists())
        self.repo.gc_orphans()
        self.assertFalse(folder.exists())

    def test_path_traversal_blocked(self):
        e = self.repo.add("安全测试")
        with self.assertRaises(StoreError):
            self.repo.store.resolve("../../windows/win.ini")

    def test_corrupted_file_recovers_from_backup(self):
        self.repo.add("重要数据")
        self.repo.save()  # 写入 .bak
        self.repo.store.events_path.write_text("{ 这不是 JSON", encoding="utf-8")
        repo2 = Repository(self.dir)
        self.assertEqual([e.title for e in repo2.all()], ["重要数据"])
        self.assertTrue(repo2.load_warnings)


class TestFormats(unittest.TestCase):
    def test_parse_datetime(self):
        self.assertEqual(parse_datetime("2026-11-23"), datetime(2026, 11, 23))
        self.assertEqual(parse_datetime("2026/11/23 08:05"), datetime(2026, 11, 23, 8, 5))
        self.assertEqual(parse_datetime("2026-11-23T08:05:09"), datetime(2026, 11, 23, 8, 5, 9))
        self.assertIsNone(parse_datetime(""))
        self.assertIsNone(parse_datetime("不是时间"))
        self.assertIsInstance(parse_datetime("12-01"), datetime)

    def test_humanize(self):
        now = datetime(2026, 11, 23, 9, 0)
        self.assertEqual(humanize_due(datetime(2026, 11, 23, 18, 0), at=now), "今天 18:00")
        self.assertEqual(humanize_due(datetime(2026, 11, 24, 9, 0), at=now), "明天 09:00")
        self.assertIn("逾期", humanize_due(datetime(2026, 11, 23, 8, 30), at=now))
        self.assertEqual(humanize_due(None), "无期限")

    def test_reminder_window(self):
        at = datetime(2026, 11, 23, 9, 0)
        e = Event("到点提醒", due=at + timedelta(minutes=5), remind_before=10)
        self.assertTrue(e.needs_reminder(at))
        e.last_reminded_at = at
        self.assertFalse(e.needs_reminder(at))
        done = Event("已完成", due=at, done=True)
        self.assertFalse(done.needs_reminder(at))
        self.assertIn("到点提醒", reminder_text([e], at))


class TestImportExport(TempRepo):
    def test_json_roundtrip(self):
        self.repo.add("甲", due="2026-12-01 08:00", tags=["t"])
        self.repo.add("乙")
        out = self.dir / "backup.json"
        importer.export_json(self.repo, out)
        other = Repository(self.dir / "other")
        result = importer.import_json(other, out)
        self.assertEqual(result["added"], 2)
        self.assertEqual([e.title for e in other.all()], ["甲", "乙"])
        # 再次导入去重
        again = importer.import_json(other, out)
        self.assertEqual(again["added"], 0)

    def test_todotxt_roundtrip(self):
        self.repo.add("写周报", due="2026-11-24T18:00", tags=["工作"])
        second = self.repo.add("买咖啡豆", tags=["生活"])
        self.repo.set_done(second.id)
        out = self.dir / "todo.txt"
        importer.export_todotxt(self.repo, out)
        text = out.read_text(encoding="utf-8")
        self.assertIn("(A)", text)
        self.assertTrue(any(line.startswith("x ") for line in text.splitlines()))
        other = Repository(self.dir / "other")
        importer.import_todotxt(other, out)
        titles = [e.title for e in other.all()]
        self.assertEqual(sorted(titles), sorted(["写周报", "买咖啡豆"]))
        done = [e for e in other.all() if e.done]
        self.assertEqual(len(done), 1)

    def test_bundle_with_attachment(self):
        e = self.repo.add("带图")
        img = self.dir / "pic.svg"
        img.write_text("<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'/>", encoding="utf-8")
        self.repo.add_attachment(e.id, str(img))
        bundle = self.dir / "pack.mjnote"
        importer.export_bundle(self.repo, bundle)
        other = Repository(self.dir / "other")
        result = importer.import_bundle(other, bundle)
        self.assertEqual(result["added"], 1)
        imported = other.all()[0]
        self.assertEqual(len(imported.attachments), 1)
        path = other.attachment_path(imported.attachments[0])
        self.assertTrue(path.is_file())
        self.assertIn("svg", path.read_text(encoding="utf-8"))

    def test_markdown_export(self):
        self.repo.add("Markdown 任务", note="第一行\n第二行", due="2026-12-02T10:00")
        out = self.dir / "out.md"
        importer.export_markdown(self.repo, out)
        text = out.read_text(encoding="utf-8")
        self.assertIn("# 墨记 · 记事本导出", text)
        self.assertIn("### 1. [ ] Markdown 任务", text)
        self.assertIn("> 第一行", text)

    def test_import_replace(self):
        self.repo.add("旧数据")
        src = self.dir / "new.json"
        src.write_text(json.dumps({"events": [{"title": "新数据"}]}, ensure_ascii=False), encoding="utf-8")
        importer.import_json(self.repo, src, merge=False)
        self.assertEqual([e.title for e in self.repo.all()], ["新数据"])


if __name__ == "__main__":
    unittest.main(verbosity=2)

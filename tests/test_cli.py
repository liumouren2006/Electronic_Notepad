"""命令行入口测试。"""

from __future__ import annotations

import io
import sys
import tempfile
import unittest
from contextlib import redirect_stdout, redirect_stderr
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from moji.cli import main  # noqa: E402
from moji.core import Repository  # noqa: E402


def run(*args: str) -> tuple[int, str]:
    out = io.StringIO()
    err = io.StringIO()
    with redirect_stdout(out), redirect_stderr(err):
        code = main(list(args))
    return code, out.getvalue() + err.getvalue()


class TestCLI(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory(prefix="moji-cli-test-")
        self.dir = self._tmp.name

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def test_add_list_done_undo(self) -> None:
        code, out = run("-d", self.dir, "add", "买牛奶", "-u", "2026-12-01T10:00", "-t", "生活")
        self.assertEqual(code, 0)
        self.assertIn("已添加 P1", out)

        code, out = run("-d", self.dir, "add", "写报告", "-p", "1")
        self.assertEqual(code, 0)
        repo = Repository(self.dir)
        self.assertEqual([e.title for e in repo.all()], ["写报告", "买牛奶"])
        self.assertEqual(repo.all()[1].priority, 2)

        code, out = run("-d", self.dir, "list")
        self.assertIn("买牛奶", out)
        self.assertEqual(code, 0)

        code, out = run("-d", self.dir, "done", "2")
        self.assertEqual(code, 0)
        self.assertTrue(Repository(self.dir).all()[1].done)

        code, out = run("-d", self.dir, "undo")
        self.assertEqual(code, 0)
        self.assertFalse(Repository(self.dir).all()[1].done)

        code, out = run("-d", self.dir, "undo", "--redo")
        self.assertEqual(code, 0)
        self.assertTrue(Repository(self.dir).all()[1].done)

    def test_list_by_date(self) -> None:
        run("-d", self.dir, "add", "纪念日", "-u", "2026-12-24T19:00")
        code, out = run("-d", self.dir, "list", "--date", "2026-12-24")
        self.assertEqual(code, 0)
        self.assertIn("纪念日", out)
        code, out = run("-d", self.dir, "list", "--date", "2026-12-25")
        self.assertEqual(code, 0)
        self.assertNotIn("纪念日", out)
        code, out = run("-d", self.dir, "list", "--date", "不是日期")
        self.assertEqual(code, 1)

    def test_search_and_invalid_id(self) -> None:
        run("-d", self.dir, "add", "课程设计报告", "-t", "课程")
        code, out = run("-d", self.dir, "search", "设计")
        self.assertEqual(code, 0)
        self.assertIn("课程设计报告", out)
        code, out = run("-d", self.dir, "show", "99")
        self.assertEqual(code, 1)
        self.assertIn("失败", out)

    def test_export_import_roundtrip(self) -> None:
        run("-d", self.dir, "add", "带附件任务")
        repo = Repository(self.dir)
        src = Path(self.dir) / "a.txt"
        src.write_text("hello", encoding="utf-8")
        repo.add_attachment(repo.all()[0].id, str(src))
        bundle = Path(self.dir) / "pack.mjnote"
        code, out = run("-d", self.dir, "export", str(bundle))
        self.assertEqual(code, 0)
        self.assertTrue(bundle.is_file())

        other = Path(self.dir) / "other"
        code, out = run("-d", str(other), "import", str(bundle))
        self.assertEqual(code, 0)
        self.assertIn("新增 1 条", out)
        imported = Repository(other).all()[0]
        self.assertEqual(imported.title, "带附件任务")
        self.assertEqual(len(imported.attachments), 1)

    def test_seed_demo_and_doctor(self) -> None:
        code, out = run("-d", self.dir, "seed-demo")
        self.assertEqual(code, 0)
        self.assertEqual(len(Repository(self.dir).all()), 10)
        code, out = run("-d", self.dir, "doctor")
        self.assertEqual(code, 0)
        self.assertIn("数据检查：通过", out)
        code, out = run("-d", self.dir, "stats")
        self.assertIn("完成率", out)


if __name__ == "__main__":
    unittest.main(verbosity=2)

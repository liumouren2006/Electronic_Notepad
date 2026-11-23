"""TUI 集成测试（Textual 的 headless pilot）。"""

from __future__ import annotations

import sys
import tempfile
import unittest
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from textual.widgets import Input, ListView  # noqa: E402

from moji.core import Repository  # noqa: E402
from moji.tui.app import MojiTUI  # noqa: E402
from moji.tui.widgets import AttachmentScreen, ConfirmScreen, EventFormScreen, HelpScreen, PathScreen  # noqa: E402


class TestTUI(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory(prefix="moji-tui-test-")
        self.repo = Repository(self._tmp.name)
        self.repo.add(
            "今天的会议",
            due=datetime.now().replace(hour=23, minute=0, second=0, microsecond=0),
            tags=["工作"],
        )
        self.repo.add("写代码", tags=["code"])

    async def asyncTearDown(self) -> None:
        self._tmp.cleanup()

    async def _rows(self, app) -> int:
        return len(app.query_one("#list", ListView).children)

    async def test_mount_filters_and_search(self) -> None:
        app = MojiTUI(self.repo)
        async with app.run_test(size=(130, 40)) as pilot:
            await pilot.pause(0.2)
            self.assertEqual(await self._rows(app), 2)
            await pilot.press("t")
            await pilot.pause(0.2)
            self.assertEqual(app.filter_mode, "today")
            self.assertEqual(await self._rows(app), 1)
            await pilot.press("0")
            await pilot.pause(0.2)
            await pilot.press("slash")
            await pilot.pause(0.2)
            self.assertIsInstance(app.focused, Input)
            await pilot.press("c", "o", "d", "e")
            await pilot.pause(0.5)
            self.assertEqual(app.search_text, "code")
            await pilot.press("escape")
            await pilot.pause(0.3)
            self.assertEqual(app.search_text, "")
            self.assertEqual(await self._rows(app), 2)

    async def test_add_edit_toggle_undo_delete(self) -> None:
        app = MojiTUI(self.repo)
        async with app.run_test(size=(130, 40)) as pilot:
            await pilot.pause(0.2)

            await pilot.press("a")
            await pilot.pause(0.2)
            self.assertIsInstance(app.screen, EventFormScreen)
            form = app.screen
            form.query_one("#f-title", Input).value = "TUI 新任务"
            form.query_one("#f-due", Input).value = "2026-12-01 10:00"
            form.query_one("#f-tags", Input).value = "测试, TUI"
            await pilot.press("ctrl+s")
            await pilot.pause(0.3)
            self.assertEqual(len(self.repo.all()), 3)
            created = self.repo.all()[-1]
            self.assertEqual(created.title, "TUI 新任务")
            self.assertEqual(created.tags, ["测试", "TUI"])

            await pilot.press("e")
            await pilot.pause(0.2)
            self.assertIsInstance(app.screen, EventFormScreen)
            app.screen.query_one("#f-title", Input).value = "TUI 改名"
            await pilot.press("ctrl+s")
            await pilot.pause(0.3)
            self.assertEqual(self.repo.get(created.id).title, "TUI 改名")

            before = len(self.repo.finished())
            await pilot.press("space")
            await pilot.pause(0.3)
            self.assertEqual(len(self.repo.finished()), before + 1)
            await pilot.press("ctrl+z")
            await pilot.pause(0.3)
            self.assertEqual(len(self.repo.finished()), before)

            await pilot.press("x")
            await pilot.pause(0.2)
            self.assertIsInstance(app.screen, ConfirmScreen)
            await pilot.press("enter")
            await pilot.pause(0.3)
            self.assertIsNone(self.repo.get(created.id))
            await pilot.press("ctrl+z")
            await pilot.pause(0.3)
            self.assertIsNotNone(self.repo.get(created.id))

    async def test_priority_move_and_help(self) -> None:
        app = MojiTUI(self.repo)
        async with app.run_test(size=(130, 40)) as pilot:
            await pilot.pause(0.2)
            first = self.repo.all()[0]
            await pilot.press("]")
            await pilot.pause(0.3)
            self.assertEqual(self.repo.get(first.id).priority, 2)
            await pilot.press("[")
            await pilot.pause(0.3)
            self.assertEqual(self.repo.get(first.id).priority, 1)

            await pilot.press("question_mark")
            await pilot.pause(0.2)
            self.assertIsInstance(app.screen, HelpScreen)
            await pilot.press("escape")
            await pilot.pause(0.2)

    async def test_mouse_click_checkbox(self) -> None:
        app = MojiTUI(self.repo)
        async with app.run_test(size=(130, 40)) as pilot:
            await pilot.pause(0.2)
            event = self.repo.all()[0]
            self.assertFalse(event.done)
            await pilot.click(f"#row-{event.id}", offset=(1, 0))
            await pilot.pause(0.3)
            self.assertTrue(self.repo.get(event.id).done)

            await pilot.click(f"#row-{event.id}", offset=(10, 1), times=2)
            await pilot.pause(0.3)
            self.assertIsInstance(app.screen, EventFormScreen)
            await pilot.press("escape")
            await pilot.pause(0.2)

    async def test_export_modal(self) -> None:
        app = MojiTUI(self.repo)
        target = Path(self._tmp.name) / "backup.mjnote"
        async with app.run_test(size=(130, 40)) as pilot:
            await pilot.pause(0.2)
            await pilot.press("E")
            await pilot.pause(0.2)
            self.assertIsInstance(app.screen, PathScreen)
            app.screen.query_one("#p-path", Input).value = str(target)
            await pilot.press("enter")
            await pilot.pause(0.8)
            self.assertTrue(target.is_file())
            self.assertGreater(target.stat().st_size, 100)

    async def test_attachment_modal(self) -> None:
        src = Path(self._tmp.name) / "note.txt"
        src.write_text("附件", encoding="utf-8")
        event = self.repo.all()[0]
        self.repo.add_attachment(event.id, str(src))
        app = MojiTUI(self.repo)
        async with app.run_test(size=(130, 40)) as pilot:
            await pilot.pause(0.2)
            await pilot.press("o")
            await pilot.pause(0.2)
            self.assertIsInstance(app.screen, AttachmentScreen)
            await pilot.press("escape")
            await pilot.pause(0.2)


if __name__ == "__main__":
    unittest.main(verbosity=2)

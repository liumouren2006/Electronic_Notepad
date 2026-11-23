"""GUI 冒烟测试。

在无显示器环境下使用 Qt offscreen 平台；如果未安装 PySide6 则跳过。
"""

from __future__ import annotations

import os
import sys
import tempfile
import unittest
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

try:
    from PySide6.QtCore import QDateTime
    from PySide6.QtGui import QImage
    from PySide6.QtWidgets import QApplication

    from moji.core import Repository
    from moji.gui.app import MainWindow

    HAVE_QT = True
except ImportError:  # pragma: no cover
    HAVE_QT = False


@unittest.skipUnless(HAVE_QT, "未安装 PySide6")
class TestGUI(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory(prefix="moji-gui-test-")
        self.dir = Path(self._tmp.name)
        self.repo = Repository(self.dir)
        self.repo.add("今天的任务", due="2026-12-01T18:00", tags=["课程"])
        self.repo.add("无期限任务")
        self.window = MainWindow(self.repo)
        self.window.show()
        self.app.processEvents()

    def tearDown(self) -> None:
        self.window.close()
        self.app.processEvents()
        self.window.deleteLater()
        self.app.processEvents()
        self._tmp.cleanup()

    def test_loaded_and_save(self) -> None:
        self.assertIsNotNone(self.window.current_id)
        first = self.repo.get(self.window.current_id)
        self.assertEqual(self.window.title_edit.text(), first.title)
        self.assertEqual(len(self.window._cards), 2)

        self.window.title_edit.setText("GUI 改标题")
        self.assertTrue(self.window.save_current())
        self.assertEqual(self.repo.get(first.id).title, "GUI 改标题")

    def test_new_event_and_autosave_fields(self) -> None:
        before = len(self.repo.all())
        self.window.new_event()
        self.assertEqual(len(self.repo.all()), before + 1)
        new_id = self.window.current_id
        self.window.title_edit.setText("GUI 新任务")
        self.window.due_check.setChecked(True)
        self.window.due_edit.setDateTime(QDateTime.fromString("2026-12-09 08:30", "yyyy-MM-dd HH:mm"))
        self.window.tags_edit.setText("测试, GUI")
        self.window.note_edit.setPlainText("自动保存测试")
        self.assertTrue(self.window.save_current())
        event = self.repo.get(new_id)
        self.assertEqual(event.title, "GUI 新任务")
        self.assertEqual(event.tags, ["测试", "GUI"])
        self.assertEqual(event.note, "自动保存测试")
        self.assertEqual(event.due, QDateTime.fromString("2026-12-09 08:30", "yyyy-MM-dd HH:mm").toPython())

    def test_filters_and_search(self) -> None:
        self.window.set_filter("today")
        self.app.processEvents()
        self.assertEqual(len(self.window._cards), len(self.repo.today()))
        self.window.set_filter("all")
        self.window.search_edit.setText("无期限")
        self.window._do_search_refresh()
        self.app.processEvents()
        self.assertEqual(len(self.window._cards), 1)
        self.window.search_edit.setText("")
        self.window._do_search_refresh()
        self.app.processEvents()
        self.assertEqual(len(self.window._cards), 2)

    def test_attachment_paste_path(self) -> None:
        image_path = self.dir / "paste.png"
        image = QImage(120, 80, QImage.Format_ARGB32)
        image.fill(0xFF4F6EF7)
        self.assertTrue(image.save(str(image_path)))
        self.window.attach_files([str(image_path)])
        event = self.repo.get(self.window.current_id)
        self.assertEqual(len(event.attachments), 1)
        self.assertEqual(self.window.att_list.count(), 1)
        self.assertTrue(event.attachments[0].mime.startswith("image/"))

    def test_clipboard_image_paste(self) -> None:
        image = QImage(64, 48, QImage.Format_ARGB32)
        image.fill(0xFF16A34A)
        QApplication.clipboard().setImage(image)
        self.window.note_edit.setFocus()
        self.window.note_edit.paste()
        self.app.processEvents()
        event = self.repo.get(self.window.current_id)
        self.assertEqual(len(event.attachments), 1)
        self.assertEqual(self.window.att_list.count(), 1)

    def test_theme_toggle(self) -> None:
        self.window.set_theme("dark")
        self.assertEqual(self.window.theme_name, "dark")
        self.window.set_theme("light")
        self.assertEqual(self.window.theme_name, "light")
        self.assertEqual(len(self.window._cards), 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)

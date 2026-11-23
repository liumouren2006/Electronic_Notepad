"""演示数据：方便第一次运行/截图/答辩展示。

附件用标准库现场生成（SVG 图片 + WAV 提示音 + TXT 附件），
不依赖任何素材文件，也不会往仓库里塞二进制。
"""

from __future__ import annotations

import math
import struct
import tempfile
import wave
from datetime import timedelta
from pathlib import Path

from .core.models import now
from .core.repository import Repository


def _make_svg(path: Path) -> None:
    svg = """<svg xmlns="http://www.w3.org/2000/svg" width="640" height="360" viewBox="0 0 640 360">
  <defs>
    <linearGradient id="g" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0%" stop-color="#5b8def"/>
      <stop offset="55%" stop-color="#7c5cff"/>
      <stop offset="100%" stop-color="#ff7eb3"/>
    </linearGradient>
  </defs>
  <rect width="640" height="360" rx="28" fill="url(#g)"/>
  <circle cx="540" cy="80" r="46" fill="#ffffff" opacity="0.18"/>
  <circle cx="110" cy="300" r="70" fill="#ffffff" opacity="0.10"/>
  <text x="48" y="150" font-family="Microsoft YaHei, Noto Sans CJK SC, sans-serif"
        font-size="52" font-weight="700" fill="#ffffff">墨记 MoJi</text>
  <text x="50" y="205" font-family="Microsoft YaHei, Noto Sans CJK SC, sans-serif"
        font-size="24" fill="#ffffff" opacity="0.9">TUI + GUI · 优先级链表 · 附件随身带</text>
  <text x="50" y="255" font-family="Microsoft YaHei, Noto Sans CJK SC, sans-serif"
        font-size="18" fill="#ffffff" opacity="0.75">这是演示附件：拖进来即可随事件一起保存</text>
</svg>
"""
    path.write_text(svg, encoding="utf-8")


def _make_wav(path: Path) -> None:
    rate = 16000
    seconds = 1.2
    frames = []
    for i in range(int(rate * seconds)):
        t = i / rate
        # 两声短促的“叮”
        env = math.exp(-8 * (t % 0.6)) if (t % 0.6) < 0.45 else 0.0
        sample = math.sin(2 * math.pi * 880 * t) * 0.35 * env
        frames.append(struct.pack("<h", int(sample * 32767)))
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(rate)
        wf.writeframes(b"".join(frames))


def seed_demo(repo: Repository) -> int:
    t = now()
    day = t.replace(hour=0, minute=0, second=0)
    entries = [
        dict(
            title="提交《电子记事本》课程设计报告",
            note=(
                "## 交付清单\n"
                "- [x] 核心功能：增删改查 + 优先级顺延\n"
                "- [x] TUI（textual）与 MD 风格 GUI（PySide6）\n"
                "- [ ] 把 **开发日志.md** 附在报告后面\n\n"
                "> 提醒：导出 .mjnote 并拷到 U 盘"
            ),
            due=day + timedelta(hours=18),
            tags=["课程", "截止"],
            remind_before=60,
            color="#5b8def",
        ),
        dict(
            title="复习《数据结构》链表章节",
            note="重点：单链表插入/删除的指针操作，以及「插入后优先级顺延」的实现。",
            due=day + timedelta(days=1, hours=10),
            tags=["学习"],
            color="#7c5cff",
        ),
        dict(
            title="跑步 3 公里",
            note="配速 6'30\"，跑完拉伸 10 分钟。",
            due=day + timedelta(hours=20),
            tags=["运动"],
            color="#26a69a",
        ),
        dict(
            title="给妈妈打电话",
            due=day + timedelta(days=2, hours=19, minutes=30),
            tags=["生活"],
        ),
        dict(
            title="整理实验数据并画图",
            note="数据在 `D:\\实验\\数据.csv`，用 Matplotlib 画折线图。",
            due=day - timedelta(days=1, hours=2),
            tags=["课程"],
            color="#ef5350",
        ),
        dict(
            title="阅读《人月神话》第 3 章",
            tags=["阅读"],
        ),
        dict(
            title="完成服务器巡检",
            due=day - timedelta(days=1, hours=6),
            tags=["运维"],
        ),
        dict(
            title="买牛奶",
            tags=["生活"],
            color="#ffa726",
        ),
        dict(
            title="准备课程答辩 PPT",
            note="结构：需求分析 → 数据结构设计 → 双界面演示 → 容错与日志。",
            due=day + timedelta(days=4, hours=14),
            tags=["课程", "答辩"],
            remind_before=120,
        ),
        dict(
            title="整理桌面文件",
            tags=["杂事"],
        ),
    ]
    created = []
    for item in entries:
        created.append(
            repo.add(
                item["title"],
                note=item.get("note", ""),
                due=item.get("due"),
                tags=item.get("tags", []),
                remind_before=item.get("remind_before", 0),
                color=item.get("color", ""),
            )
        )
    # 第 5 条标记完成
    if len(created) > 6:
        repo.set_done(created[6].id, True)
    # 给第一条加附件
    with tempfile.TemporaryDirectory(prefix="moji-demo-") as td:
        tmp = Path(td)
        svg = tmp / "moji-cover.svg"
        _make_svg(svg)
        repo.add_attachment(created[0].id, str(svg))
        wav = tmp / "ding.wav"
        _make_wav(wav)
        repo.add_attachment(created[2].id, str(wav))
        txt = tmp / "答辩提纲.txt"
        txt.write_text(
            "一、需求分析\n二、数据结构：优先级单向链表\n三、存储：UTF-8 JSON + 附件目录\n"
            "四、TUI / GUI 双界面\n五、容错与自动备份\n六、演示\n",
            encoding="utf-8",
        )
        repo.add_attachment(created[8].id, str(txt))
    return len(created)

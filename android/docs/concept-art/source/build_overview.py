from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

OUT = Path(__file__).parent


def screen_path(name: str) -> Path:
    """优先使用 source/ 下新渲染的图，否则复用 docs/concept-art/ 里的成品。"""
    local = OUT / name
    if local.exists():
        return local
    aliases = {
        "home.png": "01-home.png", "alarm.png": "02-alarm.png",
        "detail.png": "03-detail.png", "editor.png": "04-editor.png",
        "brand.png": "05-brand.png", "reminder.png": "06-reminder.png",
    }
    return OUT.parent / aliases.get(name, name)


W, H = 2000, 2420

img = Image.new("RGB", (W, H), "#0a1020")
px = img.load()
c1, c2, c3 = (9, 15, 32), (20, 42, 66), (8, 14, 27)
for y in range(H):
    t = y / H
    if t < 0.55:
        k = t / 0.55
        col = tuple(int(c1[i] + (c2[i] - c1[i]) * k) for i in range(3))
    else:
        k = (t - 0.55) / 0.45
        col = tuple(int(c2[i] + (c3[i] - c2[i]) * k) for i in range(3))
    for x in range(0, W, 4):
        for dx in range(4):
            if x + dx < W:
                px[x + dx, y] = col

d = ImageDraw.Draw(img, "RGBA")
glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
gd = ImageDraw.Draw(glow)
gd.ellipse((-300, -500, 900, 700), fill=(79, 110, 247, 34))
gd.ellipse((1300, 900, 2500, 2100), fill=(18, 165, 148, 20))
gd.ellipse((700, 1500, 1700, 2500), fill=(124, 92, 255, 14))
glow = glow.filter(ImageFilter.GaussianBlur(180))
img = Image.alpha_composite(img.convert("RGBA"), glow).convert("RGB")
d = ImageDraw.Draw(img, "RGBA")
for x in range(0, W, 100):
    d.line((x, 0, x, H), fill=(255, 255, 255, 6), width=1)
for y in range(0, H, 100):
    d.line((0, y, W, y), fill=(255, 255, 255, 6), width=1)

F = r"C:\Windows\Fonts\msyh.ttc"
FB = r"C:\Windows\Fonts\msyhbd.ttc"
f_title = ImageFont.truetype(FB, 66)
f_sub = ImageFont.truetype(F, 26)
f_tag = ImageFont.truetype(F, 17)
f_label = ImageFont.truetype(FB, 27)
f_cap = ImageFont.truetype(F, 17)
f_footer = ImageFont.truetype(F, 16)
f_index = ImageFont.truetype(FB, 18)

logo = Image.open(OUT / "assets/logo.png").convert("RGB").resize((106, 106), Image.Resampling.LANCZOS)
mask = Image.new("L", (106, 106), 0)
ImageDraw.Draw(mask).rounded_rectangle((0, 0, 105, 105), radius=25, fill=255)
img.paste(logo, (116, 82), mask)
d.text((250, 78), "墨记 MoJi", font=f_title, fill=(255, 255, 255, 255))
d.text((254, 158), "Android 1.1 · 概念界面设计", font=f_sub, fill=(184, 198, 226, 235))

tags = ["全屏闹钟", "双渠道铃声", "Markdown 渲染", "品牌与 Logo"]
x = W - 120
for t in reversed(tags):
    w = d.textlength(t, font=f_tag) + 26
    x -= w
    d.rounded_rectangle((x, 104, x + w, 142), radius=19, fill=(255, 255, 255, 16),
                        outline=(255, 255, 255, 34), width=1)
    d.text((x + 13, 112), t, font=f_tag, fill=(220, 228, 246, 235))
    x -= 10
d.text((W - 120, 162), "C O N C E P T   S H E E T", font=ImageFont.truetype(F, 14),
       fill=(126, 151, 205, 220), anchor="ra")
d.line((116, 218, W - 120, 218), fill=(255, 255, 255, 26), width=1)

screens = [
    ("home.png", "01", "首页", "自定义品牌 · 四维统计"),
    ("alarm.png", "02", "全屏闹钟", "锁屏点亮 · 循环铃声"),
    ("detail.png", "03", "事项详情", "Markdown 渲染 · 源码切换"),
    ("editor.png", "04", "编辑事项", "每事项铃声 · 草稿保护"),
    ("brand.png", "05", "品牌与标题", "Logo 裁剪 · GIF · 形状"),
    ("reminder.png", "06", "提醒设置", "? 帮助 · 音量与权限"),
]
PH_H = 920
for i, (fn, num, name, cap) in enumerate(screens):
    p = Image.open(screen_path(fn)).convert("RGB")
    w = round(p.width * PH_H / p.height)
    p = p.resize((w, PH_H), Image.Resampling.LANCZOS)
    col, row = i % 3, i // 3
    x = round(186 + col * (w + 186))
    y = 300 + row * 1030
    shadow = Image.new("RGBA", (w + 80, PH_H + 100), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.rounded_rectangle((40, 46, 40 + w, 46 + PH_H), radius=38, fill=(0, 0, 0, 150))
    shadow = shadow.filter(ImageFilter.GaussianBlur(28))
    img.paste(shadow, (x - 40, y - 18), shadow)
    mask = Image.new("L", (w, PH_H), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, w - 1, PH_H - 1), radius=34, fill=255)
    img.paste(p, (x, y), mask)
    d.rounded_rectangle((x, y, x + w - 1, y + PH_H - 1), radius=34,
                        outline=(255, 255, 255, 55), width=2)
    ly = y + PH_H + 24
    d.text((x, ly), num, font=f_index, fill=(126, 151, 205, 255))
    d.text((x + 38, ly - 4), name, font=f_label, fill=(255, 255, 255, 245))
    d.text((x + 2, ly + 34), cap, font=f_cap, fill=(172, 187, 216, 225))

d.line((116, H - 108, W - 120, H - 108), fill=(255, 255, 255, 22), width=1)
d.text((116, H - 86), "MoJi · Android Concept Art · 2026-11-25", font=f_footer,
       fill=(142, 161, 195, 220))
d.text((W - 120, H - 86), "Rendered at 390 × 844 @3x · 概念稿，并非真机截图", font=f_footer,
       fill=(142, 161, 195, 220), anchor="ra")

img.save(OUT / "overview.png")
print("overview.png", img.size)

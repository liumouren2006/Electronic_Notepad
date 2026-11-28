from pathlib import Path

OUT = Path(__file__).parent

ICONS = {
    "search": '<circle cx="11" cy="11" r="7"/><path d="M16.5 16.5 21 21"/>',
    "sync": '<path d="M20 7v5h-5"/><path d="M4 17v-5h5"/><path d="M6.1 9A7 7 0 0 1 19.5 7.5"/><path d="M17.9 15A7 7 0 0 1 4.5 16.5"/>',
    "cloud": '<path d="M7 18h10a4 4 0 0 0 .6-8A6 6 0 0 0 6 11.5 3.2 3.2 0 0 0 7 18Z"/>',
    "settings": '<path fill="currentColor" stroke="none" d="M19.43 12.98c.04-.32.07-.65.07-.98s-.03-.66-.07-.98l2.11-1.65c.19-.15.24-.42.12-.64l-2-3.46c-.12-.22-.39-.3-.61-.22l-2.49 1c-.52-.4-1.08-.73-1.69-.98l-.38-2.65C14.46 2.18 14.25 2 14 2h-4c-.25 0-.46.18-.49.42l-.38 2.65c-.61.25-1.17.59-1.69.98l-2.49-1c-.23-.09-.49 0-.61.22l-2 3.46c-.13.22-.07.49.12.64l2.11 1.65c-.04.32-.07.65-.07.98s.03.66.07.98l-2.11 1.65c-.19.15-.24.42-.12.64l2 3.46c.12.22.39.3.61.22l2.49-1c.52.4 1.08.73 1.69.98l.38 2.65c.03.24.24.42.49.42h4c.25 0 .46-.18.49-.42l.38-2.65c.61-.25 1.17-.59 1.69-.98l2.49 1c.23.09.49 0 .61-.22l2-3.46c.12-.22.07-.49-.12-.64l-2.11-1.65ZM12 15.5A3.5 3.5 0 1 1 12 8.5a3.5 3.5 0 0 1 0 7Z"/>',
    "undo": '<path d="M9 7 4 12l5 5"/><path d="M4 12h9a7 7 0 0 1 7 7"/>',
    "redo": '<path d="m15 7 5 5-5 5"/><path d="M20 12h-9a7 7 0 0 0-7 7"/>',
    "plus": '<path d="M12 5v14M5 12h14"/>',
    "more": '<circle cx="5" cy="12" r="1.3" fill="currentColor" stroke="none"/><circle cx="12" cy="12" r="1.3" fill="currentColor" stroke="none"/><circle cx="19" cy="12" r="1.3" fill="currentColor" stroke="none"/>',
    "clock": '<circle cx="12" cy="12" r="8.5"/><path d="M12 7v5.5l3.5 2"/>',
    "bell": '<path d="M18 9a6 6 0 1 0-12 0c0 6-2 7-2 7h16s-2-1-2-7"/><path d="M10.5 20a2 2 0 0 0 3 0"/>',
    "clip": '<path d="M20 11.5 11 20.5a5 5 0 0 1-7-7l9.5-9.5a3.5 3.5 0 0 1 5 5l-9.5 9.5a2 2 0 0 1-3-3L15 6"/>',
    "edit": '<path d="M4 20h4L20 8l-4-4L4 16v4Z"/><path d="m14 6 4 4"/>',
    "trash": '<path d="M4 7h16M9 7V4h6v3M6 7l1 13h10l1-13"/><path d="M10 11v5M14 11v5"/>',
    "back": '<path d="M15 5 8 12l7 7"/>',
    "check-circle": '<circle cx="12" cy="12" r="8.5"/><path d="m8 12 3 3 5-6"/>',
    "circle": '<circle cx="12" cy="12" r="8.5"/>',
    "image": '<rect x="3" y="4" width="18" height="16" rx="2"/><circle cx="8.5" cy="9" r="1.5"/><path d="m4 17 5-5 4 4 3-3 4 4"/>',
    "music": '<path d="M9 18V6l10-2v12"/><circle cx="6.5" cy="18" r="2.5"/><circle cx="16.5" cy="16" r="2.5"/>',
    "volume": '<path d="M4 9v6h4l5 4V5L8 9H4Z"/><path d="M16 9a4 4 0 0 1 0 6"/>',
    "help": '<circle cx="12" cy="12" r="8.5"/><path d="M9.5 9a2.5 2.5 0 1 1 3.5 2.3c-.7.4-1 1-1 1.7v.5"/><circle cx="12" cy="17" r=".8" fill="currentColor" stroke="none"/>',
    "folder": '<path d="M3 6h6l2 2h10v11H3V6Z"/>',
    "tag": '<path d="M3 12V4h8l10 10-8 8L3 12Z"/><circle cx="7.5" cy="8.5" r="1.3"/>',
    "list": '<path d="M8 6h12M8 12h12M8 18h12"/><path d="M4 6h.01M4 12h.01M4 18h.01"/>',
    "close": '<path d="m6 6 12 12M18 6 6 18"/>',
    "alarm": '<circle cx="12" cy="13" r="7"/><path d="M12 9v4l2.5 1.5M5 4 3 6M19 4l2 2"/>',
    "snooze": '<circle cx="12" cy="12" r="8.5"/><path d="M12 7v5l3 2"/><path d="M5 3 3 5M19 3l2 2"/>',
    "check": '<path d="m5 12 4.5 4.5L19 7"/>',
    "sliders": '<path d="M4 7h9M17 7h3M4 12h3M11 12h9M4 17h11M19 17h1"/><circle cx="15" cy="7" r="2"/><circle cx="9" cy="12" r="2"/><circle cx="17" cy="17" r="2"/>',
}

SPRITE = '<svg style="display:none" xmlns="http://www.w3.org/2000/svg">' + "".join(
    f'<symbol id="i-{k}" viewBox="0 0 24 24">{v}</symbol>' for k, v in ICONS.items()
) + '</svg>'


def ic(name, cls="icon"):
    return f'<svg class="{cls}" viewBox="0 0 24 24"><use href="#i-{name}"/></svg>'


def status(dark=False):
    return f'''
  <div class="statusbar {'dark' if dark else ''}">
    <div>17:20</div>
    <div class="status-right">
      <svg width="15" height="11" viewBox="0 0 16 11" fill="currentColor"><rect x="0" y="7" width="3" height="4" rx="1"/><rect x="4.3" y="5" width="3" height="6" rx="1"/><rect x="8.6" y="2.7" width="3" height="8.3" rx="1"/><rect x="12.9" y="0" width="3" height="11" rx="1"/></svg>
      <svg width="14" height="11" viewBox="0 0 24 19" fill="currentColor"><path d="M12 16.2 9.6 13.8a3.4 3.4 0 0 1 4.8 0L12 16.2Zm0-5.7c-2.3 0-4.3.9-5.8 2.4L4.5 11.2A11.4 11.4 0 0 1 12 8.3c2.9 0 5.5 1.1 7.5 2.9l-1.7 1.7a8.3 8.3 0 0 0-5.8-2.4Zm0-5.3c-3.7 0-7 1.5-9.4 3.9L1 6.8A15.6 15.6 0 0 1 12 2.4c4.4 0 8.3 1.8 11 4.4l-1.7 1.7A13.3 13.3 0 0 0 12 5.2Z"/></svg>
      <div class="battery"><i></i></div>
    </div>
  </div>'''


def head(title):
    return f'''<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><title>{title}</title>
<link rel="stylesheet" href="style.css"></head><body>
{SPRITE}
'''


def nav():
    return f'''
  <div class="bottom-nav">
    <div class="nav-item on"><div class="nav-pill">{ic('list')}</div><div>任务</div></div>
    <div class="nav-item"><div class="nav-pill">{ic('search')}</div><div>搜索</div></div>
    <div class="nav-item"><div class="nav-pill">{ic('settings')}</div><div>设置</div></div>
  </div>
  <div class="fab">{ic('plus','icon')}</div>
  <div class="gesture"></div>
'''


def card(emoji, title, badge, badge_cls, stripe, note, due, due_cls, tags, attach, prio, done=False):
    note_html = f'<div class="note-preview">{note}</div>' if note else ''
    attach_html = f'<div class="meta-item">{ic("clip","icon-xs")}<span>{attach}</span></div>' if attach else ''
    tags_html = ''.join(f'<span>#{t}</span>' for t in tags)
    cls = 'card done' if done else 'card'
    cb = ic('check-circle','icon') if done else ic('circle','icon')
    return f'''
    <div class="{cls}">
      <div class="stripe" style="background:{stripe}"></div>
      <div class="card-body">
        <div class="card-top">
          <div class="cb">{cb}</div>
          <div class="card-main">
            <div class="title-line">
              <span class="emoji">{emoji}</span>
              <span class="task-title">{title}</span>
              <span class="badge {badge_cls}">{badge}</span>
              <span class="more">{ic('more','icon-sm')}</span>
            </div>
            {note_html}
            <div class="card-meta">
              <div class="meta-item {due_cls}">{ic('clock','icon-xs')}<span>{due}</span></div>
              <div class="meta-item">{ic('bell','icon-xs')}</div>
              {attach_html}
              <div class="meta-item tags">{tags_html}</div>
              <span class="prio-text" style="margin-left:auto">P{prio}</span>
            </div>
          </div>
        </div>
      </div>
    </div>'''


# ---------------------------------------------------------------- home
home = head("墨记 · 首页") + f'''
<div class="screen">
  <div class="home-header">
    <div class="header-inner">
      {status(False)}
      <div class="brand-row">
        <div class="brand-logo"><img src="assets/logo.png" alt="logo"></div>
        <div class="brand-text">
          <div class="brand-name">墨记 MoJi</div>
          <div class="brand-sub">把日子写清楚</div>
        </div>
        <div class="header-actions">
          <div class="ibtn">{ic('undo','icon-sm')}</div>
          <div class="ibtn">{ic('redo','icon-sm')}</div>
          <div class="ibtn">{ic('sync','icon-sm')}</div>
          <div class="ibtn">{ic('settings','icon-sm')}</div>
        </div>
      </div>
      <div class="greeting">下午好，十一月25日 · 周三</div>
      <div class="stats">
        <div class="stat"><div class="v">4</div><div class="k">今日安排</div></div>
        <div class="stat"><div class="v">5</div><div class="k">待办</div></div>
        <div class="stat"><div class="v">1</div><div class="k">逾期</div></div>
        <div class="stat"><div class="v">58%</div><div class="k">完成率</div></div>
      </div>
      <div class="progress"><i></i></div>
      <div class="sync-line">已连接：课程资料 · 点击右上角云图标同步</div>
    </div>
  </div>

  <div class="home-content">
    <div class="search">{ic('search')}<span>搜索标题、备注、标签…</span></div>
    <div class="chips">
      <div class="chip on">全部</div>
      <div class="chip">待办</div>
      <div class="chip">今天</div>
      <div class="chip">逾期</div>
      <div class="chip">已完成</div>
    </div>
    <div class="tags">
      <div class="tagchip on">全部标签</div>
      <div class="tagchip">#课程<b>3</b></div>
      <div class="tagchip">#课程设计<b>2</b></div>
      <div class="tagchip">#实验<b>2</b></div>
      <div class="tagchip">#阅读<b>1</b></div>
    </div>
    <div class="list-head">
      <div class="list-title">全部任务</div>
      <div class="count-pill">5</div>
      <div class="clear-done">清除已完成</div>
    </div>
''' + card(
    "📌", "提交课程设计报告", "今天", "today", "#12a594",
    "提交前确认核心功能、导出与 3 分钟演示视频…",
    "今天 18:00", "today", ["课程", "截止"], "3", 2,
) + card(
    "💡", "小组会议纪要", "逾期", "overdue", "#d64545",
    "会议结论：先做离线优先，再评估同步方案…",
    "昨天 20:00", "overdue", ["课程设计"], "0", 3,
) + card(
    "✅", "整理实验数据", "已完成", "done", "#c6ccdc",
    "", "昨天 16:30", "", ["实验"], "2", 4, done=True,
) + f'''
  </div>
  {nav()}
</div>
</body></html>
'''
(OUT / "home.html").write_text(home, encoding="utf-8")


# ---------------------------------------------------------------- alarm
alarm = head("墨记 · 全屏闹钟") + f'''
<div class="screen alarm-screen">
  {status(False)}
  <div class="alarm-close">{ic('close','icon')}</div>
  <div class="alarm-content">
    <div class="alarm-top">
      <div class="alarm-glyph">{ic('alarm')}</div>
      <div class="alarm-clock">17:20</div>
      <div class="alarm-date">周三 · 2026-11-25</div>
    </div>
    <div class="alarm-card">
      <div class="alarm-title-row"><span class="alarm-emoji">📌</span><span class="alarm-title">提交课程设计报告</span></div>
      <div class="alarm-due">截止 2026-11-25 18:00（40 分钟后）</div>
      <div class="alarm-note">
        <b>验收清单</b><br>
        核心功能 / 页面墨迹 / .mjnote 导出<br>
        演示视频控制在 3 分钟以内。
      </div>
    </div>
    <div class="alarm-spacer"></div>
    <div class="alarm-actions">
      <div class="abtn mute">{ic('volume','icon-sm')}<span>静音</span></div>
      <div class="abtn snooze">{ic('snooze','icon-sm')}<span>5 分钟后再提醒</span></div>
    </div>
    <div class="abtn primary">{ic('check','icon-sm')}<span>完成并关闭</span></div>
    <div class="alarm-link">查看详情并停止铃声</div>
    <div class="alarm-brand">M O J I &nbsp; · &nbsp; A L A R M</div>
  </div>
</div>
</body></html>
'''
(OUT / "alarm.html").write_text(alarm, encoding="utf-8")


# ---------------------------------------------------------------- detail
def mrow(icon_name, label, value, cls=""):
    return f'<div class="mrow">{ic(icon_name,"icon-sm")}<div class="label">{label}</div><div class="value {cls}">{value}</div></div>'


def taskline(done, text):
    icon = ic('check-circle' if done else 'circle', 'icon-sm')
    color = 'color:#4f6ef7' if done else 'color:#8b93a7'
    return f'<div class="task { "done" if done else "" }"><span style="{color}">{icon}</span><span>{text}</span></div>'


detail = head("墨记 · 事项详情") + f'''
<div class="screen">
  {status(True)}
  <div class="topbar">
    <div class="ibtn">{ic('back','icon')}</div>
    <div class="title">事项详情</div>
    <div class="ibtn" style="color:#4f6ef7">{ic('edit','icon')}</div>
    <div class="ibtn" style="color:#d64545">{ic('trash','icon')}</div>
  </div>
  <div class="tabrow">
    <div class="tab on">详情</div>
    <div class="tab">页面与墨迹</div>
    <div class="tab-indicator"><i></i></div>
  </div>
  <div class="detail-scroll">
    <div class="dcard">
      <div class="dt-head">
        <div class="dt-stripe"></div>
        <div class="dt-title">📌 提交课程设计报告</div>
      </div>
      <div class="pills">
        <span class="pill blue">今天</span>
        <span class="pill blue">P2</span>
        <span class="pill gray">3 个附件</span>
      </div>
      <div class="dt-buttons">
        <div class="btn primary">{ic('check-circle','icon-sm')}标记为完成</div>
        <div class="btn outline">{ic('edit','icon-sm')}编辑</div>
      </div>
    </div>
    <div class="dcard">
      {mrow('clock','截止时间','2026-11-25 18:00（今天 18:00）')}
      {mrow('bell','提醒','提前 30 分钟 · 稍后 17:45')}
      {mrow('music','铃声','晨间.mp3')}
      {mrow('sliders','优先级','P2（第 2 位）')}
      {mrow('tag','标签','#课程  #截止')}
    </div>
    <div class="dcard">
      <div class="section-head">
        <div class="h">备注</div>
        <div class="mini-chip on">渲染</div>
        <div class="mini-chip">源码</div>
      </div>
      <div class="md">
        <h2>验收清单</h2>
        <p>提交前请确认以下内容，报告文件使用 <code>PDF</code> 格式。</p>
        {taskline(True, '核心功能与页面墨迹')}
        {taskline(True, '导出 .mjnote 与 Markdown')}
        {taskline(False, '录制 3 分钟演示视频')}
        <blockquote>演示视频控制在 3 分钟以内，重点展示闹钟与同步。</blockquote>
        <table>
          <tr><th>项目</th><th>状态</th><th>负责人</th></tr>
          <tr><td>核心功能</td><td>完成</td><td>LiBertY6IO</td></tr>
          <tr><td>演示视频</td><td>进行中</td><td>LiBertY6IO</td></tr>
        </table>
      </div>
    </div>
    <div class="dcard">
      <div class="section-head"><div class="h">附件 · 3</div></div>
      <div class="att-row">
        <div class="thumb">{ic('image','icon')}</div>
        <div><div class="att-name">验收截图.png</div><div class="att-sub">image/png · 286 KB</div></div>
      </div>
    </div>
  </div>
  <div class="gesture"></div>
</div>
</body></html>
'''
(OUT / "detail.html").write_text(detail, encoding="utf-8")


# ---------------------------------------------------------------- brand settings + dialog
brand = head("墨记 · 品牌与标题") + f'''
<div class="screen">
  {status(True)}
  <div class="settings-header">
    <div class="settings-title">设置</div>
    <div class="settings-sub">外观、品牌、同步、导入导出与提醒</div>
  </div>
  <div class="settings-scroll">
    <div class="section">
      <div class="section-head">
        <div class="section-icon">{ic('image','icon-sm')}</div>
        <div class="section-title">品牌与标题</div>
        <div class="help">?</div>
      </div>
      <div class="field"><div class="flabel">应用名称</div><div class="ftext">墨记 MoJi</div></div>
      <div class="field"><div class="flabel">副标题</div><div class="ftext">把日子写清楚</div></div>
      <div class="field-row">
        <div class="btn primary" style="background:#e9edff;color:#4f6ef7;box-shadow:none">应用标题</div>
        <div class="btn outline">恢复默认</div>
      </div>
      <div class="brand-preview-row">
        <div class="brand-preview"><img src="assets/logo.png" alt="logo"></div>
        <div class="brand-preview-info"><div class="t">Logo 图片</div><div class="s">logo.png · 墨迹印章</div></div>
        <div class="link-btn" style="margin:0">移除</div>
      </div>
      <div class="field-row" style="margin-top:10px">
        <div class="btn outline">更换图片</div>
        <div class="btn primary" style="background:#eef1ff;color:#4f6ef7;box-shadow:none">裁剪与位置</div>
      </div>
    </div>
    <div class="section" style="opacity:.55">
      <div class="section-head">
        <div class="section-icon">{ic('sliders','icon-sm')}</div>
        <div class="section-title">外观</div>
        <div class="help">?</div>
      </div>
      <div class="setting-row"><div class="texts"><div class="t">主题</div><div class="s">跟随系统 / 浅色 / 深色</div></div><div class="trail">浅色</div></div>
    </div>
  </div>
  <div class="scrim"></div>
  <div class="dialog">
    <div class="dialog-title">裁剪与位置</div>
    <div class="dialog-preview">
      <div class="logo-circle"><img src="assets/logo.png" alt="logo preview"></div>
    </div>
    <div class="dialog-hint">拖动图片平移；滑块缩放；GIF 会按原动画播放。</div>
    <div class="shape-chips">
      <div class="shape-chip">圆角</div>
      <div class="shape-chip on">圆形</div>
      <div class="shape-chip">方形</div>
      <div class="shape-chip">六边形</div>
      <div class="shape-chip">拉伸铺满</div>
    </div>
    <div class="zoom-row">
      <div class="zlabel">缩放</div>
      <div class="slider"><i style="width:72%"></i><span class="knob" style="left:72%"></span></div>
      <div class="pct">120%</div>
    </div>
    <div class="dialog-actions">
      <div class="text-btn">更换图片</div>
      <div class="text-btn" style="color:#7a8498">重置</div>
      <div class="grow"></div>
      <div class="text-btn" style="color:#7a8498">取消</div>
      <div class="filled-btn">保存</div>
    </div>
  </div>
</div>
</body></html>
'''
(OUT / "brand.html").write_text(brand, encoding="utf-8")


# ---------------------------------------------------------------- reminder settings + help
def setting_row(icon_name, title, sub, trail="", trail_cls=""):
    return f'''<div class="setting-row">
      <div class="icon-box">{ic(icon_name,'icon-sm')}</div>
      <div class="texts"><div class="t">{title}</div><div class="s">{sub}</div></div>
      <div class="{trail_cls}">{trail}</div>
    </div>'''


reminder = head("墨记 · 提醒设置") + f'''
<div class="screen">
  {status(True)}
  <div class="settings-header">
    <div class="settings-title">设置</div>
    <div class="settings-sub">外观、品牌、同步、导入导出与提醒</div>
  </div>
  <div class="settings-scroll">
    <div class="section">
      <div class="section-head">
        <div class="section-icon">{ic('bell','icon-sm')}</div>
        <div class="section-title">提醒</div>
        <div class="help">?</div>
      </div>
      {setting_row('bell','通知权限','已允许，到期会弹出全屏闹钟','已允许','check')}
      <div class="divider"></div>
      <div style="font-size:10.5px;color:#525c72;margin-bottom:6px">默认铃声</div>
      <div class="setting-row" style="background:#fafbff;border-radius:14px;padding:8px 10px;min-height:52px">
        <div class="icon-box">{ic('music','icon-sm')}</div>
        <div class="texts"><div class="t">晨间.mp3</div><div class="s">来自本机音频 · 1.2 MB</div></div>
        <div class="trail">试听</div>
      </div>
      <div class="field-row" style="margin-top:8px">
        <div class="btn outline">更换铃声</div>
        <div class="btn outline">使用系统铃声</div>
      </div>
      <div class="vol-head"><span>铃声音量</span><b>78%</b></div>
      <div class="slider"><i style="width:78%"></i><span class="knob" style="left:78%"></span></div>
      <div class="divider"></div>
      {setting_row('clock','精确闹钟','已开启，按秒准时提醒','已开启','check')}
      {setting_row('alarm','全屏闹钟','已开启，锁屏也会点亮','已开启','check')}
    </div>
    <div class="section" style="opacity:.55">
      <div class="section-head">
        <div class="section-icon">{ic('folder','icon-sm')}</div>
        <div class="section-title">数据</div>
        <div class="help">?</div>
      </div>
      <div class="setting-row"><div class="texts"><div class="t">记录 12 条 · 待办 5 条 · 附件 8 个</div><div class="s">自动备份 20 份</div></div></div>
    </div>
  </div>
  <div class="scrim"></div>
  <div class="help-dialog">
    <h3>提醒</h3>
    <p>锁屏/息屏时会点亮屏幕并弹出全屏闹钟页，循环播放铃声；设备正在使用时会弹出带系统闹钟铃声的高优先级通知，点击通知进入闹钟页。</p>
    <p>通知上可直接「停止铃声」或「查看详情」。铃声优先使用该事项的铃声，未设置则使用默认铃声，再没有就用系统闹钟铃声。</p>
    <div class="ok">知道了</div>
  </div>
</div>
</body></html>
'''
(OUT / "reminder.html").write_text(reminder, encoding="utf-8")



# ---------------------------------------------------------------- editor
editor = head("墨记 · 编辑事项") + f"""
<div class="screen">
  {status(True)}
  <div class="topbar" style="height:60px">
    <div class="ibtn">{ic('back','icon')}</div>
    <div style="flex:1;margin-left:3px">
      <div class="edit-title">编辑待办</div>
      <div class="edit-sub">尚未保存</div>
    </div>
    <div class="save-text">保存修改</div>
  </div>
  <div class="edit-scroll">
    <div class="field" style="margin-bottom:12px"><div class="flabel">标题</div><div class="ftext">提交课程设计报告</div></div>
    <div class="section" style="padding:14px 16px">
      <div class="section-head" style="margin-bottom:10px">
        <div class="mini-chip on">Markdown</div>
        <div class="mini-chip">预览</div>
        <span class="chars">128 字符</span>
      </div>
      <div class="note-editor"><span class="h">## 验收清单</span>
- [x] 核心功能与页面墨迹
- [x] 导出 .mjnote 与 Markdown
- [ ] 录制 3 分钟演示视频

&gt; 演示视频控制在 3 分钟以内。</div>
    </div>
    <div class="section" style="padding:14px 16px">
      <div class="setting-row">
        <div class="icon-box" style="background:#eef1ff">{ic('clock','icon-sm')}</div>
        <div class="texts"><div class="t">2026-11-25 18:00</div><div class="s">截止时间 · 今天</div></div>
        <div class="mini-actions"><span class="gray">清除</span></div>
      </div>
      <div class="divider"></div>
      <div class="setting-row">
        <div class="icon-box" style="background:#eef1ff">{ic('bell','icon-sm')}</div>
        <div class="texts"><div class="t">提前 30 分钟提醒</div><div class="s">到期时还会再提醒一次</div></div>
        <div class="mini-actions"><span>30 分钟</span></div>
      </div>
      <div class="divider"></div>
      <div class="setting-row">
        <div class="icon-box" style="background:#eef1ff">{ic('music','icon-sm')}</div>
        <div class="texts"><div class="t">该事项铃声</div><div class="s">晨间.mp3</div></div>
        <div class="mini-actions"><span>更换</span><span class="gray">清除</span></div>
      </div>
      <div class="divider"></div>
      <div class="setting-row">
        <div class="icon-box" style="background:#eef1ff">{ic('bell','icon-sm')}</div>
        <div class="texts"><div class="t">PC 端系统通知</div><div class="s">同步到 PC 后是否在 Windows 通知中心弹出</div></div>
        <div class="switch"></div>
      </div>
    </div>
    <div class="section" style="padding:14px 16px">
      <div class="e-label">标签</div>
      <div class="tags" style="margin-top:0">
        <div class="tagchip on">#课程</div>
        <div class="tagchip on">#截止</div>
        <div class="tagchip">#实验</div>
        <div class="tagchip">#阅读</div>
      </div>
    </div>
    <div class="field-row" style="margin-top:14px">
      <div class="btn outline" style="height:44px">取消</div>
      <div class="btn primary" style="height:44px;flex:2">保存修改</div>
    </div>
  </div>
  <div class="gesture"></div>
</div>
</body></html>
"""
(OUT / "editor.html").write_text(editor, encoding="utf-8")


import base64
LOGO = "data:image/png;base64," + base64.b64encode((OUT / "assets/logo.png").read_bytes()).decode()
for name in ["home", "alarm", "detail", "editor", "brand", "reminder"]:
    f = OUT / f"{name}.html"
    f.write_text(f.read_text(encoding="utf-8").replace("assets/logo.png", LOGO), encoding="utf-8")
print("pages written")

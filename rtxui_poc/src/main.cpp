// 墨记 MoJi · RTXUI TUI PoC（独立分支 feature/rtxui-tui）
//
// 这是用 RTXUI（HTML/CSS 驱动的 C++23 TUI）重写的实验版界面。它复用
// src/core 的 Repository/Event，不改动 src/tui 的 Win32 TUI。
//
// 构建见 rtxui_poc/README.md。

#include <rtxui/rtxui.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#include "core/model.hpp"
#include "core/repository.hpp"
#include "core/util.hpp"

using namespace rtxui;
using moji::Repository;

namespace {

Repository* g_repo = nullptr;
Screen* g_screen = nullptr;

std::string qs(const std::string& s) { return s; }

std::string short_tags(const moji::Event* e) {
    if (!e) return "";
    std::string out;
    for (size_t i = 0; i < e->tags.size() && i < 2; ++i) {
        out += " #" + e->tags[i];
    }
    return out;
}

std::string priority_color(int p) {
    static const char* colors[] = {"", "#f87171", "#fb923c", "#facc15", "#4ade80",
                                   "#2dd4bf", "#60a5fa", "#a78bfa", "#f472b6",
                                   "#94a3b8"};
    int i = std::max(1, std::min(9, p));
    return colors[i];
}

struct Row {
    std::string id;
    std::string title;
    std::string icon;
    std::string priority;
    std::string due;
    std::string due_class;
    std::string tags;
    std::string att;
    std::string row_class;
    std::string accent;
    std::string mark;
    std::string mark_class;

    bool operator==(const Row&) const = default;
};

void Fields(const Row& row, FieldSink& out) {
    out("id", row.id);
    out("title", row.title);
    out("icon", row.icon);
    out("priority", row.priority);
    out("due", row.due);
    out("due_class", row.due_class);
    out("tags", row.tags);
    out("att", row.att);
    out("row_class", row.row_class);
    out("accent", row.accent);
    out("mark", row.mark);
    out("mark_class", row.mark_class);
}

struct AttRow {
    std::string name;
    std::string size;
    std::string mime;
    std::string index;
    std::string missing;

    bool operator==(const AttRow&) const = default;
};

void Fields(const AttRow& row, FieldSink& out) {
    out("name", row.name);
    out("size", row.size);
    out("mime", row.mime);
    out("index", row.index);
    out("missing", row.missing);
}

std::string markdown_styles() {
    return R"css(
      h1 { color: #e6e9ef; border-bottom: solid; border-color: #2a3142; margin-bottom: 1; }
      h2 { color: #e6e9ef; margin-top: 1; border-bottom: solid; border-color: #2a3142; }
      h3 { color: #e6e9ef; margin-top: 1; }
      p { color: #c4c9d6; }
      strong { color: #ffffff; font-weight: bold; }
      em { color: #c4b5fd; }
      u { text-decoration: underline; }
      s { text-decoration: line-through; }
      mark { background-color: #44391d; color: #fde68a; }
      code { color: #2dd4bf; background-color: #121722; }
      pre { background-color: #121722; border: tall; border-color: #232b3e; padding: 0 1; }
      blockquote { border-left: heavy; border-color: #4b5563; padding-left: 2; color: #8b93a7; }
      ul, ol { margin-left: 2; color: #c4c9d6; }
      a { color: #5b8def; text-decoration: underline; }
      table { border: solid; border-color: #2a3550; margin-top: 1; margin-bottom: 1; }
      th { font-weight: bold; color: #7c9cff; border-bottom: solid; border-color: #2a3550; padding: 0 1; }
      td { padding: 0 1; }
    )css";
}

constexpr std::string_view kHelpMd = R"md(
# 墨记 · 快捷键

## 浏览

| 按键 | 作用 |
| --- | --- |
| ↑ ↓ / k j | 上下选择 |
| Enter / e | 编辑选中事件 |
| Space / d | 勾选 / 取消勾选 |
| Delete / x | 删除（会先确认） |
| a | 新建事件 |
| / | 搜索 |
| o | 查看附件 |
| [ / ] | 提高 / 降低优先级 |
| Ctrl+Z / Ctrl+Y | 撤销 / 重做 |
| ? | 本帮助 |
| q | 退出 |

新建/编辑表单：Tab / ↑↓ 切换字段，Ctrl+S 保存，Esc 取消。
)md";

class MojiApp : public Component<MojiApp> {
public:
    MojiApp() {
        Bind(items);
        Bind(attachments);
        Bind(search);
        Bind(search_active);
        Bind(search_inactive);
        Bind(search_display);
        Bind(clock_text);
        Bind(status);
        Bind(tag_summary);
        Bind(has_selection);
        Bind(detail_title);
        Bind(detail_meta);
        Bind(detail_note);
        Bind(detail_mark);
        Bind(detail_mark_class);
        Bind(stats_today);
        Bind(stats_pending);
        Bind(stats_overdue);
        Bind(stats_rate);
        Bind(count_all);
        Bind(count_today);
        Bind(count_pending);
        Bind(count_done);
        Bind(count_overdue);
        Bind(nav_all);
        Bind(nav_today);
        Bind(nav_pending);
        Bind(nav_done);
        Bind(nav_overdue);
        Bind(help_open);
        Bind(help_md);
        Bind(markdown_css);
        Bind(has_attachments);
        Bind(empty_list);
        Bind(form_open);
        Bind(form_heading);
        Bind(form_title);
        Bind(form_due);
        Bind(form_remind);
        Bind(form_pos);
        Bind(form_tags);
        Bind(form_note);
        Bind(form_notify);
        Bind(form_error);
        Bind(attach_open);
        Bind(confirm_open);
        Bind(confirm_text);
        Bind(Select);
        Bind(StartSearch);
        Bind(StopSearch);
        Bind(ModeAll);
        Bind(ModeToday);
        Bind(ModePending);
        Bind(ModeDone);
        Bind(ModeOverdue);
        Bind(OpenAdd);
        Bind(OpenEdit);
        Bind(OpenHelp);
        Bind(CloseHelp);
        Bind(OpenAttach);
        Bind(CloseAttach);
        Bind(OpenAttachment);
        Bind(RemoveAttachment);
        Bind(ToggleSelected);
        Bind(OpenConfirmDelete);
        Bind(ConfirmDelete);
        Bind(CancelConfirm);
        Bind(MoveUp);
        Bind(MoveDown);
        Bind(SaveForm);
        Bind(CancelForm);
        Bind(QuickToday);
        Bind(QuickTomorrow);
        Bind(QuickNextWeek);
        Bind(QuickClear);
        Bind(Undo);
        Bind(Redo);
        Bind(Quit);
        if (g_repo && !g_repo->all().empty()) {
            selected_id = g_repo->all().front()->id;
        }
    }

    bool Digest() override {
        rebuild();
        return Component<MojiApp>::Digest();
    }

    bool OnEvent(Event event) override {
        if (event == Event::CtrlS() && form_open) {
            SaveForm();
            return true;
        }
        if (event == Event::Escape()) {
            if (form_open) { form_open = false; form_error.clear(); return true; }
            if (help_open) { help_open = false; return true; }
            if (attach_open) { attach_open = false; return true; }
            if (confirm_open) { confirm_open = false; return true; }
            if (search_active) {
                search_active = false;
                search.clear();
                return true;
            }
            return true;
        }
        if (form_open || help_open || attach_open || confirm_open) {
            return Component<MojiApp>::OnEvent(event);
        }
        if (search_active) {
            return Component<MojiApp>::OnEvent(event);
        }

        auto typed = [&](char c) {
            return event == Event(Event::Keyboard::From(c));
        };
        if (typed('q')) { Quit(); return true; }
        if (typed('a')) { OpenAdd(); return true; }
        if (typed('e') || event == Event::Return()) { OpenEdit(); return true; }
        if (typed('x') || event == Event::Delete()) { OpenConfirmDelete(); return true; }
        if (typed(' ') || typed('d')) { ToggleSelected(); return true; }
        if (typed('o')) { OpenAttach(); return true; }
        if (typed('?')) { OpenHelp(); return true; }
        if (typed('/')) { StartSearch(); return true; }
        if (typed('[')) { MoveUp(); return true; }
        if (typed(']')) { MoveDown(); return true; }
        if (typed('t')) { ModeToday(); return true; }
        if (typed('0')) { ModeAll(); return true; }
        if (typed('1')) { ModePending(); return true; }
        if (typed('2')) { ModeDone(); return true; }
        if (typed('3')) { ModeOverdue(); return true; }
        if (event == Event::ArrowUp() || typed('k')) { move_selection(-1); return true; }
        if (event == Event::ArrowDown() || typed('j')) { move_selection(1); return true; }
        if (event == Event::CtrlZ()) { Undo(); return true; }
        if (event == Event::CtrlY()) { Redo(); return true; }
        return Component<MojiApp>::OnEvent(event);
    }

    std::string_view view = R"html(
      <div class="app">
        <div class="header">
          <span class="brand">◉ 墨记 MoJi</span>
          <span class="header-center">— 电子记事本 · TUI</span>
          <span class="clock">{clock_text}</span>
        </div>

        <div class="body">
          <div class="sidebar">
            <div class="stats">
              <div><span class="muted">今日</span> <span class="primary">{stats_today}</span>　　<span class="muted">逾期</span> <span class="danger">{stats_overdue}</span></div>
              <div><span class="muted">待办</span> <span class="text">{stats_pending}</span>　　<span class="muted">完成</span> <span class="success">{stats_rate}%</span></div>
            </div>

            <input class="search-input" if="{search_active}" value="{search}" placeholder="🔍 搜索标题、标签、正文…" autofocus=""/>
            <div class="search-box" if="{search_inactive}" onclick="StartSearch">{search_display}</div>

            <div class="nav-title">视图</div>
            <button class="nav {nav_all}" onclick="ModeAll">全部 <span class="nav-count">{count_all}</span></button>
            <button class="nav {nav_today}" onclick="ModeToday">今天 <span class="nav-count">{count_today}</span></button>
            <button class="nav {nav_pending}" onclick="ModePending">待办 <span class="nav-count">{count_pending}</span></button>
            <button class="nav {nav_done}" onclick="ModeDone">已完成 <span class="nav-count">{count_done}</span></button>
            <button class="nav {nav_overdue}" onclick="ModeOverdue">逾期 <span class="nav-count">{count_overdue}</span></button>

            <div class="nav-title">标签</div>
            <div class="tags">{tag_summary}</div>
            <div class="sidebar-fill"></div>
          </div>

          <div class="center">
            <div class="center-head">
              <span class="center-title">记事</span>
              <button class="primary small" onclick="OpenAdd">＋ 新建</button>
            </div>
            <div class="count-line">共 {count_all} 条 · 按优先级排序</div>
            <div class="list">
              <for each="{items}" as="item">
                <div class="item {item.row_class}" onclick="Select({$index})">
                  <div class="accent" style="background-color: {item.accent};"></div>
                  <div class="item-body">
                    <div class="line1">
                      <span class="mark {item.mark_class}">{item.mark}</span>
                      <span class="icon">{item.icon}</span>
                      <span class="item-title">{item.title}</span>
                      <span class="prio">{item.priority}</span>
                    </div>
                    <div class="line2">
                      <span class="{item.due_class}">⏱ {item.due}</span>
                      <span class="item-tags">{item.tags}</span>
                      <span class="item-att">{item.att}</span>
                    </div>
                  </div>
                </div>
              </for>
              <if condition="{empty_list}">
                <div class="empty">没有符合条件的记事</div>
              </if>
            </div>
          </div>

          <div class="detail">
            <if condition="{has_selection}">
              <div class="detail-head">
                <div class="detail-title">{detail_mark} {detail_title}</div>
              </div>
              <div class="detail-actions">
                <button onclick="OpenEdit">编辑</button>
                <button onclick="ToggleSelected">完成</button>
                <button onclick="OpenAttach">附件</button>
                <button class="danger" onclick="OpenConfirmDelete">删除</button>
              </div>
              <div class="detail-meta">{detail_meta}</div>
              <div class="detail-scroll">
                <markdown content="{detail_note}" stylesheet="{markdown_css}"/>
                <if condition="{has_attachments}">
                  <div class="att-title">附件</div>
                  <for each="{attachments}" as="att">
                    <div class="att-line">⧉ {att.name}  <span class="muted">{att.size}</span></div>
                  </for>
                </if>
              </div>
            <else>
              <div class="detail-empty">未选择记事</div>
              <div class="detail-hint">按 a 新建，或从左侧选择一条记事。</div>
            </else>
            </if>
          </div>
        </div>

        <div class="footer">
          <span>↑↓ 选择　Space 勾选　a 新增　e 编辑　x 删除　[ ] 调序　t 今日　/ 搜索　E 导出　? 帮助　q 退出</span>
          <span class="status">{status}</span>
        </div>

        <if condition="{form_open}">
        <dialog open="true" title="{form_heading}">
            <input value="{form_title}" placeholder="标题：要做什么？" autofocus=""/>
            <div class="form-row">
              <input value="{form_due}" placeholder="截止：2026-11-24 18:00（留空=无期限）"/>
              <input class="w-sm" value="{form_remind}" placeholder="提前提醒(分钟)"/>
              <input class="w-sm" value="{form_pos}" placeholder="优先级位置"/>
            </div>
            <div class="quick-row">
              <button class="quick" onclick="QuickToday">今天 18:00</button>
              <button class="quick" onclick="QuickTomorrow">明天 09:00</button>
              <button class="quick" onclick="QuickNextWeek">下周一 09:00</button>
              <button class="quick" onclick="QuickClear">清除时间</button>
            </div>
            <input value="{form_tags}" placeholder="标签：课程, 截止（逗号分隔，可留空）"/>
            <textarea value="{form_note}" placeholder="备注（支持 Markdown，Ctrl+S 保存）"/>
            <checkbox checked="{form_notify}">提醒时使用 Windows 系统通知</checkbox>
            <div class="form-error">{form_error}</div>
            <div class="dialog-buttons">
              <button onclick="CancelForm">取消 (Esc)</button>
              <button class="primary" onclick="SaveForm">保存 (Ctrl+S)</button>
            </div>
        </dialog>
        </if>

        <if condition="{help_open}">
        <dialog open="true" title="? 使用帮助">
          <div class="dialog-heading">墨记 · 快捷键</div>
          <div class="help-scroll">
            <markdown content="{help_md}" stylesheet="{markdown_css}"/>
          </div>
          <div class="dialog-buttons"><button class="primary" onclick="CloseHelp">关闭 (Esc)</button></div>
        </dialog>
        </if>

        <if condition="{attach_open}">
        <dialog open="true" title="📎 附件">
          <for each="{attachments}" as="att">
            <div class="att-row">
              <span class="att-name">{att.name}</span>
              <span class="muted">{att.size} · {att.mime}</span>
              <span class="att-buttons">
                <button onclick="OpenAttachment({att.index})">打开</button>
                <button class="danger" onclick="RemoveAttachment({att.index})">移除</button>
              </span>
            </div>
          </for>
          <div class="dialog-buttons"><button onclick="CloseAttach">关闭</button></div>
        </dialog>
        </if>

        <if condition="{confirm_open}">
        <dialog open="true" title="删除记事">
          <p>{confirm_text}</p>
          <div class="dialog-buttons">
            <button onclick="CancelConfirm">取消</button>
            <button class="danger" onclick="ConfirmDelete">删除</button>
          </div>
        </dialog>
        </if>
      </div>

      <style>
        self {
          width: 100%;
          height: 100%;
          background-color: #0b0e14;
          color: #e6e9ef;
        }
        .app { display: flex; flex-direction: column; width: 100%; height: 100%; overflow: hidden; padding-bottom: 1; }
        .header {
          height: 1;
          background-color: #0f1320;
          display: flex;
          justify-content: space-between;
          padding: 0 1;
          color: #e6e9ef;
        }
        .brand { color: #e6e9ef; font-weight: bold; }
        .header-center { color: #8b93a7; }
        .clock { color: #8b93a7; }
        .body { flex: 1; display: flex; min-height: 0; overflow: hidden; }
        .sidebar {
          width: 44;
          min-height: 0;
          overflow: hidden;
          background-color: #0d111c;
          border-right: solid;
          border-color: #1d2433;
          padding: 0 1;
          display: flex;
          flex-direction: column;
        }
        .stats { padding: 1 0; color: #8b93a7; }
        .search-box, .search-input {
          height: 3;
          background-color: #121722;
          border: tall;
          border-color: #232b3e;
          padding: 0 1;
          color: #8b93a7;
          margin-bottom: 1;
        }
        .search-box:focus, .search-input:focus { border-color: #5b8def; color: #e6e9ef; }
        .nav-title { padding: 1 0 0 0; color: #68708a; }
        .nav {
          height: 1;
          text-align: left;
          background-color: transparent;
          border: none;
          color: #8b93a7;
          padding: 0 1;
        }
        .nav:hover { background-color: #151b28; color: #e6e9ef; }
        .nav.active { background-color: #1b2438; color: #ffffff; }
        .nav-count { color: #8b93a7; }
        .tags { color: #a78bfa; padding: 0 1; }
        .sidebar-fill { flex: 1; }
        .center { width: 36; min-height: 0; display: flex; flex-direction: column; background-color: #0b0e14; }
        .center-head { height: 3; padding: 0 1; display: flex; justify-content: space-between; }
        .center-title { color: #e6e9ef; font-weight: bold; padding-top: 1; }
        .count-line { height: 1; padding: 0 1; color: #8b93a7; }
        .list { flex: 1; overflow-y: auto; }
        .item { height: 2; display: flex; padding: 0 1; }
        .item:hover { background-color: #151b28; }
        .item.selected { background-color: #1b2438; }
        .accent { width: 1; margin-right: 1; }
        .item-body { flex: 1; display: flex; flex-direction: column; }
        .line1 { height: 1; display: flex; gap: 1; }
        .line2 { height: 1; color: #8b93a7; }
        .item-title { flex: 1; color: #e6e9ef; }
        .mark { color: #56607a; }
        .mark.done { color: #4ade80; }
        .icon { color: #2dd4bf; }
        .prio { color: #7c9cff; }
        .due { color: #8b93a7; }
        .due.overdue { color: #f87171; }
        .due.today { color: #fbbf24; }
        .due.muted { color: #56607a; }
        .item-tags { color: #a78bfa; }
        .item-att { color: #2dd4bf; }
        .empty { color: #56607a; padding: 1; }
        .detail { flex: 1; min-height: 0; display: flex; flex-direction: column; padding: 1 2; min-width: 30; }
        .detail-head { display: flex; justify-content: space-between; }
        .detail-title { flex: 1; color: #ffffff; font-weight: bold; }
        .detail-actions { display: flex; gap: 1; margin-bottom: 1; }
        .detail-actions button { height: 1; border: none; background-color: #1b2438; padding: 0 1; }
        .detail-meta { color: #8b93a7; margin-bottom: 1; }
        .detail-scroll { flex: 1; min-height: 0; overflow-y: auto; }
        .detail-empty { color: #56607a; }
        .detail-hint { color: #68708a; margin-top: 1; }
        .att-title { color: #2dd4bf; margin-top: 1; }
        .att-line { color: #2dd4bf; }
        .footer {
          height: 1;
          flex: none;
          position: fixed;
          left: 0;
          bottom: 0;
          width: 100%;
          background-color: #0f1320;
          color: #8b93a7;
          display: flex;
          justify-content: space-between;
          padding: 0 1;
        }
        .status { color: #fbbf24; }
        .muted { color: #8b93a7; }
        .primary { color: #7c9cff; }
        .text { color: #e6e9ef; }
        .success { color: #4ade80; }
        .danger { color: #f87171; }
        button { border: solid; border-color: #2a3550; background-color: #1b2438; color: #c4c9d6; padding: 0 1; }
        button:hover, button:focus { border-color: #5b8def; color: #ffffff; }
        .primary { background-color: #5b8def; border-color: #5b8def; color: #ffffff; font-weight: bold; }
        .small { height: 1; }
        dialog::part(dialog-box) {
          width: 94;
          max-width: 94;
          max-height: 90%;
          background-color: #121722;
          border: round;
          border-color: #2a3550;
          padding: 1 2;
        }
        dialog::part(dialog-header) {
          color: #7c9cff;
          border-bottom: solid;
          border-color: #2a3550;
          margin-bottom: 1;
          padding-bottom: 1;
        }
        dialog::part(dialog-body) { display: flex; flex-direction: column; }
        .dialog-heading { color: #7c9cff; font-weight: bold; text-align: center; }
        .form-row { display: flex; gap: 1; height: 3; }
        .form-row input { flex: 1; }
        .w-sm { width: 18; flex: none; }
        .quick-row { height: 2; display: flex; gap: 1; }
        .quick { height: 1; background-color: #1b2438; border: none; color: #aab3c5; }
        .quick:hover { background-color: #223052; color: #ffffff; }
        input, textarea {
          background-color: #0f1320;
          border: tall;
          border-color: #232b3e;
          color: #e6e9ef;
          padding: 0 1;
        }
        input:focus, textarea:focus { border-color: #5b8def; }
        textarea { height: 8; }
        .form-error { color: #f87171; height: 1; }
        .dialog-buttons { display: flex; justify-content: flex-end; gap: 1; }
        .help-scroll { height: 24; overflow-y: auto; }
        .att-row { height: 1; display: flex; gap: 1; align-items: center; }
        .att-name { flex: 1; color: #e6e9ef; }
        .att-buttons { display: flex; gap: 1; flex: none; }
        .att-buttons button { width: 6; height: 1; padding: 0; border: none; background-color: #1b2438; }
      </style>
    )html";

private:
    std::vector<moji::Event*> filtered() {
        std::vector<moji::Event*> base;
        if (mode == "today") base = g_repo->due_on(moji::now_time());
        else if (mode == "pending") base = g_repo->pending();
        else if (mode == "done") base = g_repo->finished();
        else if (mode == "overdue") base = g_repo->overdue();
        else base = g_repo->all();
        if (search.empty()) return base;
        std::string key = moji::lower_ascii(moji::trim(search));
        std::vector<moji::Event*> out;
        for (moji::Event* e : base) {
            std::string hay = moji::lower_ascii(e->title + " " + e->note + " " +
                                                moji::join(e->tags, " "));
            for (const auto& a : e->attachments) hay += " " + moji::lower_ascii(a.name);
            if (hay.find(key) != std::string::npos) out.push_back(e);
        }
        return out;
    }

    moji::Event* selected_event() {
        return selected_id.empty() ? nullptr : g_repo->find(selected_id);
    }

    void rebuild() {
        auto visible = filtered();
        if (visible.empty()) {
            selected_id.clear();
        } else {
            bool found = false;
            for (auto* e : visible) found = found || e->id == selected_id;
            if (!found) selected_id = visible.front()->id;
        }

        items.clear();
        for (auto* e : visible) {
            Row row;
            row.id = e->id;
            row.title = e->title;
            row.icon = e->icon;
            row.priority = "P" + std::to_string(e->priority);
            row.due = moji::humanize_due(e->due);
            row.due_class = e->done ? "muted" : e->is_overdue() ? "overdue"
                                     : e->is_due_today() ? "today" : "";
            row.tags = short_tags(e);
            row.att = e->attachments.empty() ? "" : "⧉" + std::to_string(e->attachments.size());
            row.row_class = e->id == selected_id ? "selected" : "";
            row.accent = e->color.empty() ? priority_color(e->priority) : e->color;
            row.mark = e->done ? "✔" : "◻";
            row.mark_class = e->done ? "done" : "";
            items.push_back(std::move(row));
        }

        auto stats = g_repo->stats();
        stats_today = std::to_string(stats.today);
        stats_pending = std::to_string(stats.pending);
        stats_overdue = std::to_string(stats.overdue);
        stats_rate = std::to_string(stats.rate);
        count_all = std::to_string(stats.total);
        count_today = std::to_string(stats.today);
        count_pending = std::to_string(stats.pending);
        count_done = std::to_string(stats.done);
        count_overdue = std::to_string(stats.overdue);
        nav_all = mode == "all" ? "active" : "";
        nav_today = mode == "today" ? "active" : "";
        nav_pending = mode == "pending" ? "active" : "";
        nav_done = mode == "done" ? "active" : "";
        nav_overdue = mode == "overdue" ? "active" : "";
        clock_text = moji::clock_str(moji::now_time());
        search_display = search.empty() ? "🔍 搜索标题、标签、正文…" : "🔍 " + search;
        search_inactive = !search_active;
        empty_list = items.empty();

        std::string tags;
        int n = 0;
        for (const auto& kv : g_repo->tags()) {
            if (n++ >= 4) break;
            tags += "#" + kv.first + " " + std::to_string(kv.second) + "  ";
        }
        tag_summary = tags.empty() ? "暂无标签" : tags;

        moji::Event* e = selected_event();
        has_selection = e != nullptr;
        if (e) {
            detail_title = e->title;
            detail_meta = "P" + std::to_string(e->priority) + "  ·  " +
                          moji::humanize_due(e->due);
            if (!e->tags.empty()) detail_meta += "  ·  " + short_tags(e);
            if (!e->attachments.empty()) {
                detail_meta += "  ·  ⧉" + std::to_string(e->attachments.size());
            }
            if (e->system_notify) detail_meta += "  ·  Windows 通知";
            detail_note = e->note.empty() ? "*暂无备注，按 e 编辑*" : e->note;
            detail_mark = e->done ? "✔" : "○";
            detail_mark_class = e->done ? "done" : "";
            attachments.clear();
            for (size_t i = 0; i < e->attachments.size(); ++i) {
                const auto& a = e->attachments[i];
                AttRow row;
                row.name = a.name;
                row.size = moji::human_size(a.size);
                row.mime = a.mime;
                row.index = std::to_string(i);
                row.missing = g_repo->store.attachment_exists(a) ? "" : "missing";
                attachments.push_back(std::move(row));
            }
            has_attachments = !attachments.empty();
        } else {
            detail_title.clear();
            detail_meta.clear();
            detail_note.clear();
            detail_mark.clear();
            detail_mark_class.clear();
            attachments.clear();
            has_attachments = false;
        }
    }

    void transfer_from_form(moji::Event* existing) {
        std::string title = moji::trim(form_title);
        form_error.clear();
        if (title.empty()) {
            form_error = "标题不能为空";
            return;
        }
        std::optional<std::time_t> due;
        if (!moji::trim(form_due).empty()) {
            due = moji::parse_datetime(form_due);
            if (!due.has_value()) {
                form_error = "截止时间格式不对，应为 YYYY-MM-DD HH:MM";
                return;
            }
        }
        int remind = 0;
        if (!moji::trim(form_remind).empty()) {
            try {
                size_t used = 0;
                remind = std::stoi(moji::trim(form_remind), &used);
                if (used != moji::trim(form_remind).size() || remind < 0) throw std::exception();
            } catch (...) {
                form_error = "提前提醒需要是非负整数（分钟）";
                return;
            }
        }
        std::optional<int> pos;
        if (!moji::trim(form_pos).empty()) {
            try {
                size_t used = 0;
                int p = std::stoi(moji::trim(form_pos), &used);
                if (used != moji::trim(form_pos).size() || p < 1) throw std::exception();
                pos = p;
            } catch (...) {
                form_error = "优先级位置需要是 ≥1 的整数";
                return;
            }
        }
        std::vector<std::string> tags = moji::split_tags(form_tags);
        try {
            if (existing) {
                std::string id = existing->id;
                int old = existing->priority;
                g_repo->update(id, title, form_note, due, tags, remind,
                               std::nullopt, std::nullopt, std::nullopt, form_notify);
                if (pos.has_value() && *pos != old) g_repo->move_to(id, *pos);
                selected_id = id;
                status = "已保存修改";
            } else {
                moji::Event& added =
                    g_repo->add(title, form_note, due, tags, pos, remind);
                std::string id = added.id;
                if (form_notify) {
                    moji::Event* a = g_repo->find(id);
                    g_repo->update(id, a->title, a->note, a->due, a->tags,
                                   a->remind_before, std::nullopt, std::nullopt,
                                   std::nullopt, true);
                }
                selected_id = id;
                mode = "all";
                search.clear();
                status = "已加入 P" + std::to_string(g_repo->find(id)->priority);
            }
            form_open = false;
        } catch (const std::exception& ex) {
            form_error = std::string("保存失败：") + ex.what();
        }
    }

public:
    void Select(int index) {
        if (index >= 0 && index < (int)items.size()) selected_id = items[(size_t)index].id;
    }
    void StartSearch() { search_active = true; }
    void StopSearch() { search_active = false; }
    void ModeAll() { mode = "all"; }
    void ModeToday() { mode = "today"; }
    void ModePending() { mode = "pending"; }
    void ModeDone() { mode = "done"; }
    void ModeOverdue() { mode = "overdue"; }

    void OpenAdd() {
        editing_id.clear();
        form_heading = "＋ 新建记事";
        form_title.clear();
        form_due.clear();
        form_remind = "0";
        form_pos.clear();
        form_tags.clear();
        form_note.clear();
        form_notify = false;
        form_error.clear();
        search_active = false;
        form_open = true;
    }

    void OpenEdit() {
        moji::Event* e = selected_event();
        if (!e) return;
        editing_id = e->id;
        form_heading = "✎ 编辑记事";
        form_title = e->title;
        form_due = e->due.has_value()
                       ? moji::date_str(*e->due) + " " + moji::clock_str(*e->due)
                       : "";
        form_remind = std::to_string(e->remind_before);
        form_pos = std::to_string(e->priority);
        form_tags = moji::join(e->tags, ", ");
        form_note = e->note;
        form_notify = e->system_notify;
        form_error.clear();
        search_active = false;
        form_open = true;
    }

    void OpenHelp() { help_open = true; }
    void CloseHelp() { help_open = false; }
    void OpenAttach() {
        if (!selected_event()) return;
        attach_open = true;
    }
    void CloseAttach() { attach_open = false; }

    void OpenAttachment(int index) {
        moji::Event* e = selected_event();
        if (!e || index < 0 || index >= (int)e->attachments.size()) return;
        try {
            auto path = g_repo->store.resolve(e->attachments[(size_t)index]);
            ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr,
                          SW_SHOWNORMAL);
            status = "已打开 " + e->attachments[(size_t)index].name;
        } catch (const std::exception& ex) {
            status = std::string("无法打开：") + ex.what();
        }
    }

    void RemoveAttachment(int index) {
        moji::Event* e = selected_event();
        if (!e || index < 0 || index >= (int)e->attachments.size()) return;
        std::string name = e->attachments[(size_t)index].name;
        g_repo->remove_attachment(e->id, index);
        status = "已移除附件：" + name;
        if (g_repo->find(e->id)->attachments.empty()) attach_open = false;
    }

    void ToggleSelected() {
        moji::Event* e = selected_event();
        if (!e) return;
        g_repo->toggle_done(e->id);
        status = e->done ? "已取消完成" : "已完成";
    }

    void OpenConfirmDelete() {
        moji::Event* e = selected_event();
        if (!e) return;
        pending_delete = e->id;
        confirm_text = "确定删除「" + e->title + "」吗？删除后仍可 Ctrl+Z 撤销。";
        confirm_open = true;
    }
    void CancelConfirm() { confirm_open = false; }
    void ConfirmDelete() {
        if (!pending_delete.empty()) {
            g_repo->remove(pending_delete);
            if (pending_delete == selected_id) selected_id.clear();
            status = "已删除（Ctrl+Z 可撤销）";
            pending_delete.clear();
        }
        confirm_open = false;
    }

    void MoveUp() {
        moji::Event* e = selected_event();
        if (!e) return;
        g_repo->move_by(e->id, -1);
        status = "优先级已提高";
    }
    void MoveDown() {
        moji::Event* e = selected_event();
        if (!e) return;
        g_repo->move_by(e->id, 1);
        status = "优先级已降低";
    }

    void SaveForm() {
        transfer_from_form(editing_id.empty() ? nullptr
                                              : g_repo->find(editing_id));
    }
    void CancelForm() { form_open = false; form_error.clear(); }
    void QuickToday() {
        std::time_t t = moji::start_of_day(moji::now_time()) + 18 * 3600;
        form_due = moji::date_str(t) + " " + moji::clock_str(t);
    }
    void QuickTomorrow() {
        std::time_t t = moji::start_of_day(moji::now_time() + 86400) + 9 * 3600;
        form_due = moji::date_str(t) + " " + moji::clock_str(t);
    }
    void QuickNextWeek() {
        std::time_t now = moji::now_time();
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &now);
#else
        localtime_r(&now, &tm);
#endif
        int days = (1 - tm.tm_wday + 7) % 7;
        if (days == 0) days = 7;
        std::time_t t = moji::start_of_day(now) + days * 86400 + 9 * 3600;
        form_due = moji::date_str(t) + " " + moji::clock_str(t);
    }
    void QuickClear() { form_due.clear(); }

    void Undo() {
        if (g_repo->undo()) status = "已撤销";
        else status = "没有可撤销的操作";
    }
    void Redo() {
        if (g_repo->redo()) status = "已重做";
        else status = "没有可重做的操作";
    }
    void Quit() {
        if (g_screen) g_screen->Exit();
    }

private:
    void move_selection(int delta) {
        if (items.empty()) return;
        int idx = 0;
        for (int i = 0; i < (int)items.size(); ++i) {
            if (items[(size_t)i].id == selected_id) { idx = i; break; }
        }
        idx = std::max(0, std::min((int)items.size() - 1, idx + delta));
        selected_id = items[(size_t)idx].id;
    }

    std::string selected_id;
    std::string mode = "all";
    std::string search;
    bool search_active = false;
    bool search_inactive = true;
    std::string last_search_placeholder;

    std::vector<Row> items;
    std::vector<AttRow> attachments;

    std::string search_display;
    std::string clock_text;
    std::string status;
    std::string tag_summary;
    std::string stats_today, stats_pending, stats_overdue, stats_rate;
    std::string count_all, count_today, count_pending, count_done, count_overdue;
    std::string nav_all, nav_today, nav_pending, nav_done, nav_overdue;
    bool has_selection = false;
    bool has_attachments = false;
    bool empty_list = true;
    std::string detail_title, detail_meta, detail_note, detail_mark, detail_mark_class;

    bool help_open = false;
    std::string help_md = std::string(kHelpMd);
    std::string markdown_css = markdown_styles();

    bool form_open = false;
    std::string editing_id;
    std::string form_heading, form_title, form_due, form_remind, form_pos, form_tags,
        form_note, form_error;
    bool form_notify = false;

    bool attach_open = false;
    bool confirm_open = false;
    std::string confirm_text;
    std::string pending_delete;
};

}  // namespace

int main(int argc, char** argv) {
    moji::fs::path data_dir;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "-d" || a == "--data-dir") && i + 1 < argc) {
            data_dir = moji::fs::path(moji::to_wide(argv[++i]));
        }
    }
    try {
        Repository repo(data_dir.empty() ? moji::fs::path{} : data_dir);
        g_repo = &repo;
        auto app = Ref<MojiApp>::New();
        Screen screen(app);
        g_screen = &screen;
        screen.SetInline(false);
        screen.Loop();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "墨记 RTXUI 启动失败：%s\n", e.what());
        return 1;
    }
    return 0;
}

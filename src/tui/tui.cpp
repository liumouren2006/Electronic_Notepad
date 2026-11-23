#include "tui.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "core/importer.hpp"
#include "core/util.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace moji {

#ifdef _WIN32

namespace {

std::string fg(int r, int g, int b) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "\x1b[38;2;%d;%d;%dm", r, g, b);
    return buf;
}
const char* RESET = "\x1b[0m";

std::string strip_ansi(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '[') {
            i += 2;
            while (i < s.size() && !(s[i] >= '@' && s[i] <= '~')) {
                ++i;
            }
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

int visible_width(const std::string& s) { return display_width(strip_ansi(s)); }

std::string pad_line(std::string line, int width) {
    int w = visible_width(line);
    if (w < width) {
        line += std::string((size_t)(width - w), ' ');
    }
    return line;
}

const char* C_PANEL = "\x1b[48;2;13;17;28m";
const char* C_CARD = "\x1b[48;2;27;36;56m";
const char* C_DETAIL = "\x1b[48;2;11;14;20m";
const char* C_BAR = "\x1b[48;2;15;19;32m";

struct Key {
    enum class Type {
        None,
        Char,
        Up,
        Down,
        Enter,
        Escape,
        Delete,
        PageUp,
        PageDown,
        Home,
        End,
        Undo,
        Redo,
        Mouse,
        WheelUp,
        WheelDown,
        Resize,
        Quit
    } type = Type::None;
    wchar_t ch = 0;
    int x = 0;
    int y = 0;
};

class Console {
public:
    bool init() {
        in_ = GetStdHandle(STD_INPUT_HANDLE);
        out_ = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!in_ || !out_) return false;
        GetConsoleMode(in_, &in_mode_);
        GetConsoleMode(out_, &out_mode_);
        SetConsoleOutputCP(65001);
        SetConsoleCP(65001);
        SetConsoleMode(out_, out_mode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        return true;
    }

    void shutdown() {
        if (!in_) return;
        SetConsoleMode(in_, in_mode_);
        SetConsoleMode(out_, out_mode_);
    }

    int width() const {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(out_, &info)) {
            return info.srWindow.Right - info.srWindow.Left + 1;
        }
        return 100;
    }
    int height() const {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(out_, &info)) {
            return info.srWindow.Bottom - info.srWindow.Top + 1;
        }
        return 30;
    }

    void enter_alt() {
        write("\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H\x1b[0m");
        set_raw();
    }
    void leave_alt() { write("\x1b[0m\x1b[?25h\x1b[?1049l"); }

    void set_raw() {
        SetConsoleMode(in_, ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS);
        FlushConsoleInputBuffer(in_);
    }
    void set_cooked() {
        SetConsoleMode(in_, ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT |
                               ENABLE_EXTENDED_FLAGS);
    }

    void begin_input() {
        leave_alt();
        set_cooked();
        std::cout << "\n";
    }
    void end_input() {
        set_raw();
        enter_alt();
    }

    std::string ask(const std::string& label, const std::string& def = "") {
        std::cout << label;
        if (!def.empty()) std::cout << " [" << def << "]";
        std::cout << "：";
        std::cout.flush();
        wchar_t buf[1024];
        DWORD read = 0;
        if (!ReadConsoleW(in_, buf, 1023, &read, nullptr)) return def;
        std::wstring line(buf, buf + read);
        while (!line.empty() && (line.back() == L'\r' || line.back() == L'\n')) line.pop_back();
        std::string out = to_utf8(line);
        if (out.empty() && !def.empty()) return def;
        return out;
    }

    void write(const std::string& s) {
        std::fwrite(s.data(), 1, s.size(), stdout);
        std::fflush(stdout);
    }

    void beep() {
        std::cout << "\a" << std::flush;
        MessageBeep(MB_ICONEXCLAMATION);
    }

    Key read_key(int timeout_ms) {
        if (WaitForSingleObject(in_, (DWORD)timeout_ms) == WAIT_TIMEOUT) return Key{};
        for (;;) {
            INPUT_RECORD rec;
            DWORD count = 0;
            if (!ReadConsoleInputW(in_, &rec, 1, &count) || count == 0) return Key{};
            if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT) {
                return Key{Key::Type::Resize};
            }
            if (rec.EventType == MOUSE_EVENT) {
                const MOUSE_EVENT_RECORD& m = rec.Event.MouseEvent;
                if (m.dwEventFlags == MOUSE_WHEELED) {
                    short delta = (short)HIWORD(m.dwButtonState);
                    Key k;
                    k.type = delta > 0 ? Key::Type::WheelUp : Key::Type::WheelDown;
                    return k;
                }
                if (m.dwEventFlags == 0 &&
                    (m.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)) {
                    CONSOLE_SCREEN_BUFFER_INFO info;
                    int top = 0, left = 0;
                    if (GetConsoleScreenBufferInfo(out_, &info)) {
                        top = info.srWindow.Top;
                        left = info.srWindow.Left;
                    }
                    Key k;
                    k.type = Key::Type::Mouse;
                    k.x = m.dwMousePosition.X - left;
                    k.y = m.dwMousePosition.Y - top;
                    return k;
                }
                continue;
            }
            if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;
            const KEY_EVENT_RECORD& k = rec.Event.KeyEvent;
            DWORD ctrl = k.dwControlKeyState;
            if (ctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
                if (k.wVirtualKeyCode == 'Z') return Key{Key::Type::Undo};
                if (k.wVirtualKeyCode == 'Y') return Key{Key::Type::Redo};
                if (k.wVirtualKeyCode == 'C') return Key{Key::Type::Quit};
            }
            Key out;
            switch (k.wVirtualKeyCode) {
                case VK_UP: out.type = Key::Type::Up; return out;
                case VK_DOWN: out.type = Key::Type::Down; return out;
                case VK_HOME: out.type = Key::Type::Home; return out;
                case VK_END: out.type = Key::Type::End; return out;
                case VK_PRIOR: out.type = Key::Type::PageUp; return out;
                case VK_NEXT: out.type = Key::Type::PageDown; return out;
                case VK_DELETE: out.type = Key::Type::Delete; return out;
                case VK_ESCAPE: out.type = Key::Type::Escape; return out;
                default: break;
            }
            if (k.uChar.UnicodeChar == 13) {
                out.type = Key::Type::Enter;
                return out;
            }
            if (k.uChar.UnicodeChar != 0) {
                out.type = Key::Type::Char;
                out.ch = k.uChar.UnicodeChar;
                return out;
            }
        }
    }

private:
    HANDLE in_ = nullptr;
    HANDLE out_ = nullptr;
    DWORD in_mode_ = 0;
    DWORD out_mode_ = 0;
};

class Tui {
public:
    explicit Tui(Repository& repo) : repo_(repo) {}

    int run() {
        if (!console_.init()) {
            std::cerr << "无法初始化控制台。\n";
            return 2;
        }
        console_.enter_alt();
        last_mtime_ = repo_.mtime();
        refresh_visible(true);
        tick();
        for (;;) {
            draw();
            Key key = console_.read_key(900);
            if (key.type == Key::Type::None) {
                tick();
                continue;
            }
            if (handle(key)) break;
        }
        console_.leave_alt();
        console_.shutdown();
        return 0;
    }

private:
    Repository& repo_;
    Console console_;
    std::string mode_ = "all";
    std::string search_;
    std::string selected_id_;
    std::string toast_;
    std::time_t toast_until_ = 0;
    int scroll_ = 0;
    int last_list_top_ = 6;
    int last_sidebar_w_ = 44;
    std::optional<fs::file_time_type> last_mtime_;
    std::vector<Event*> visible_;

    void toast(const std::string& text, int seconds = 3) {
        toast_ = text;
        toast_until_ = now_time() + seconds;
    }

    std::string mode_name() const {
        if (mode_ == "today") return "今日";
        if (mode_ == "pending") return "待办";
        if (mode_ == "done") return "已完成";
        if (mode_ == "overdue") return "逾期";
        return "全部";
    }

    std::vector<Event*> filter_events() {
        std::vector<Event*> base;
        if (mode_ == "today") base = repo_.due_on(now_time());
        else if (mode_ == "pending") base = repo_.pending();
        else if (mode_ == "done") base = repo_.finished();
        else if (mode_ == "overdue") base = repo_.overdue();
        else base = repo_.all();
        if (search_.empty()) return base;
        std::string key = lower_ascii(trim(search_));
        std::vector<Event*> out;
        for (Event* e : base) {
            std::string hay = lower_ascii(e->title + " " + e->note + " " + join(e->tags, " "));
            for (const auto& a : e->attachments) hay += " " + lower_ascii(a.name);
            if (hay.find(key) != std::string::npos) out.push_back(e);
        }
        return out;
    }

    void refresh_visible(bool select_first = false) {
        visible_ = filter_events();
        if (visible_.empty()) {
            selected_id_.clear();
            scroll_ = 0;
            return;
        }
        bool found = false;
        for (Event* e : visible_) {
            if (e->id == selected_id_) {
                found = true;
                break;
            }
        }
        if (!found && (select_first || selected_id_.empty())) {
            selected_id_ = visible_.front()->id;
            scroll_ = 0;
        }
    }

    Event* current() {
        return selected_id_.empty() ? nullptr : repo_.find(selected_id_);
    }

    int current_index() const {
        for (size_t i = 0; i < visible_.size(); ++i) {
            if (visible_[i]->id == selected_id_) return (int)i;
        }
        return -1;
    }

    void move_selection(int delta) {
        if (visible_.empty()) return;
        int idx = current_index();
        if (idx < 0) idx = 0;
        idx = std::max(0, std::min((int)visible_.size() - 1, idx + delta));
        selected_id_ = visible_[(size_t)idx]->id;
    }

    void tick() {
        auto due = repo_.due_reminders(now_time());
        if (!due.empty()) {
            console_.beep();
            std::string msg = "⏰ ";
            for (size_t i = 0; i < due.size() && i < 4; ++i) {
                if (i) msg += "；";
                msg += due[i]->title + "（" + humanize_due(due[i]->due) + "）";
            }
            if (due.size() > 4) msg += " 等 " + std::to_string(due.size()) + " 项";
            toast(msg, 15);
            repo_.mark_reminded(due, now_time());
            last_mtime_ = repo_.mtime();
        }
        auto mt = repo_.mtime();
        if (mt.has_value() && last_mtime_.has_value() && *mt != *last_mtime_) {
            repo_.reload();
            refresh_visible(true);
            last_mtime_ = repo_.mtime();
            toast("检测到数据被其他程序修改，已刷新", 4);
        }
        if (!toast_.empty() && now_time() > toast_until_) toast_.clear();
    }

    void sync_mtime() { last_mtime_ = repo_.mtime(); }

    void draw() {
        int w = std::max(80, console_.width() - 1);
        int h = std::max(20, console_.height() - 1);
        int sw = 44;
        if (w < 90) sw = 34;
        int dw = w - sw - 1;
        last_sidebar_w_ = sw;
        int body_h = h - 2;
        last_list_top_ = 6;

        Stats stats = repo_.stats();
        std::vector<std::string> lines;

        std::string left = std::string(C_BAR) + " " + fg(230, 233, 239) + "◉ 墨记 MoJi";
        std::string center = "— 电子记事本 · TUI";
        std::string right = clock_str(now_time()) + " ";
        int left_w = visible_width(left);
        int center_w = display_width(center);
        int right_w = visible_width(right);
        int center_x = std::max(left_w + 1, (w - center_w) / 2);
        int pad1 = std::max(1, center_x - left_w);
        int pad2 = std::max(1, w - center_x - center_w - right_w);
        std::string header = left + std::string((size_t)pad1, ' ') +
                             fg(139, 147, 167) + center +
                             std::string((size_t)pad2, ' ') + fg(139, 147, 167) + right;
        lines.push_back(pad_line(header, w) + RESET);

        std::vector<std::string> side;
        std::vector<std::string> detail;
        build_sidebar(side, sw, stats);
        build_detail(detail, dw, body_h);

        for (int i = 0; i < body_h; ++i) {
            std::string s = i < (int)side.size() ? side[(size_t)i] : std::string(C_PANEL);
            std::string d = i < (int)detail.size() ? detail[(size_t)i] : std::string(C_DETAIL);
            std::string line = pad_line(s, sw) + fg(42, 49, 66) + "│" + pad_line(d, dw);
            lines.push_back(line + RESET);
        }

        std::string footer_plain =
            "↑↓ 选择　Space 勾选　a 新增　e 编辑　x 删除　[ ] 调序　t 今日　/ 搜索　E 导出　? 帮助　q 退出";
        if (display_width(footer_plain) > w - 2) {
            footer_plain = truncate_display(footer_plain, w - 2);
        }
        lines.push_back(pad_line(std::string(C_BAR) + " " + fg(160, 170, 195) + footer_plain,
                                 w) +
                        RESET);

        std::string out = "\x1b[H";
        for (const auto& line : lines) out += line + "\x1b[K\r\n";
        out += "\x1b[J";
        console_.write(out);
    }

    void build_sidebar(std::vector<std::string>& side, int sw, const Stats& stats) {
        auto add = [&](const std::string& line) { side.push_back(pad_line(line, sw)); };
        add(std::string(C_PANEL) + " " + fg(139, 147, 167) + "今日 " + fg(124, 156, 255) +
            std::to_string(stats.today) + "　　" + fg(139, 147, 167) + "逾期 " +
            fg(stats.overdue ? 248 : 139, stats.overdue ? 113 : 147,
               stats.overdue ? 113 : 167) + std::to_string(stats.overdue));
        add(std::string(C_PANEL) + " " + fg(139, 147, 167) + "待办 " + fg(230, 233, 239) +
            std::to_string(stats.pending) + "　　" + fg(139, 147, 167) + "完成 " +
            fg(74, 222, 128) + std::to_string(stats.rate) + "%");
        std::string search_line = std::string(C_PANEL) + " " + fg(90, 100, 125) + "/ " +
                                  (search_.empty() ? "搜索标题、标签、正文…"
                                                   : fg(196, 181, 253) + "搜索：" + search_);
        add(search_line);
        add(std::string(C_PANEL) + " " + fg(124, 156, 255) + "◈ " + mode_name() +
            fg(90, 100, 125) + "  共 " + std::to_string(visible_.size()) + " 条");
        add(std::string(C_PANEL));

        int idx = current_index();
        int rows = std::max(1, (console_.height() - 2 - 6 - 2) / 2);
        if (idx >= 0) {
            if (idx < scroll_) scroll_ = idx;
            if (idx >= scroll_ + rows) scroll_ = idx - rows + 1;
        }
        scroll_ = std::max(0, std::min(scroll_, std::max(0, (int)visible_.size() - 1)));

        if (visible_.empty()) {
            add(std::string(C_PANEL) + " " + fg(90, 100, 125) + "没有符合条件的记事");
        }
        for (int i = scroll_; i < (int)visible_.size() && i < scroll_ + rows; ++i) {
            Event* e = visible_[(size_t)i];
            bool sel = (i == idx);
            const char* row_bg = sel ? C_CARD : C_PANEL;
            std::string mark = e->done ? "✔" : "◻";
            const char* mark_color =
                e->done ? "\x1b[38;2;74;222;128m" : "\x1b[38;2;107;116;136m";
            std::string title_color =
                e->done ? fg(86, 96, 122) : (sel ? fg(255, 255, 255) : fg(230, 233, 239));
            std::string line1 = std::string(row_bg) + " " + mark_color + mark + " " +
                                title_color + truncate_display(e->title, sw - 10) +
                                " " + fg(124, 156, 255) + "P" + std::to_string(e->priority);
            add(line1);
            std::string due_color;
            if (e->done) due_color = fg(86, 96, 122);
            else if (e->is_overdue()) due_color = fg(248, 113, 113);
            else if (e->is_due_today()) due_color = fg(251, 191, 36);
            else due_color = fg(139, 147, 167);
            std::string line2 = std::string(row_bg) + "   " + due_color + humanize_due(e->due);
            for (size_t t = 0; t < e->tags.size() && t < 2; ++t) {
                line2 += " " + fg(167, 139, 250) + "#" + truncate_display(e->tags[t], 8);
            }
            if (!e->attachments.empty()) {
                line2 += " " + fg(45, 212, 191) + "⧉" + std::to_string(e->attachments.size());
            }
            add(line2);
        }
        while ((int)side.size() < console_.height() - 4) add(std::string(C_PANEL));
        add(std::string(C_PANEL) + " " + fg(86, 96, 122) +
            "↑↓ 选择  Space 勾选  a 新增  ? 帮助");
        add(std::string(C_PANEL));
        while ((int)side.size() < console_.height() - 2) add(std::string(C_PANEL));
    }

    void build_detail(std::vector<std::string>& detail, int dw, int body_h) {
        auto add = [&](const std::string& line) { detail.push_back(pad_line(line, dw)); };
        Event* e = current();
        if (!e) {
            add(std::string(C_DETAIL) + " " + fg(90, 100, 125) + "未选择记事");
            add(std::string(C_DETAIL));
            add(std::string(C_DETAIL) + " " + fg(139, 147, 167) +
                "按 a 新建，或从左侧选择一条记事。");
            while ((int)detail.size() < body_h) add(std::string(C_DETAIL));
            return;
        }
        std::string mark = e->done ? "✔" : "○";
        std::string title_color = e->done ? fg(74, 222, 128) : fg(255, 255, 255);
        add(std::string(C_DETAIL) + " " + title_color + mark + " " + fg(240, 242, 248) +
            truncate_display(e->title, dw - 6));
        std::string meta_plain = "P" + std::to_string(e->priority) + "  ·  " +
                                 humanize_due(e->due);
        if (!e->tags.empty()) {
            meta_plain += "  ·  ";
            for (size_t i = 0; i < e->tags.size() && i < 3; ++i) {
                meta_plain += "#" + e->tags[i] + " ";
            }
        }
        if (!e->attachments.empty()) {
            meta_plain += "  ·  ⧉" + std::to_string(e->attachments.size()) + "（o）";
        }
        if (e->remind_before) {
            meta_plain += "  ·  提前" + std::to_string(e->remind_before) + "分";
        }
        add(std::string(C_DETAIL) + " " + fg(139, 147, 167) +
            truncate_display(meta_plain, dw - 3));
        std::string sep = std::string(C_DETAIL) + " " + fg(42, 49, 66);
        for (int i = 0; i < dw - 2; ++i) sep += "─";
        add(sep);

        if (!toast_.empty()) {
            add(std::string(C_DETAIL) + " " + fg(251, 191, 36) +
                truncate_display(toast_, dw - 3));
            add(std::string(C_DETAIL));
        }
        int remaining = body_h - (int)detail.size() - 2;
        if (e->note.empty()) {
            add(std::string(C_DETAIL) + " " + fg(90, 100, 125) + "暂无备注，按 e 编辑");
        } else {
            for (const auto& raw : split(e->note, '\n')) {
                std::string text = raw;
                std::string prefix;
                std::string color = fg(196, 201, 214);
                if (text.rfind("### ", 0) == 0) {
                    text = text.substr(4);
                    color = fg(230, 233, 239);
                } else if (text.rfind("## ", 0) == 0) {
                    text = text.substr(3);
                    color = fg(230, 233, 239);
                } else if (text.rfind("# ", 0) == 0) {
                    text = text.substr(2);
                    color = fg(230, 233, 239);
                } else if (text.rfind("- [x] ", 0) == 0) {
                    text = text.substr(6);
                    prefix = "☑ ";
                    color = fg(139, 147, 167);
                } else if (text.rfind("- [ ] ", 0) == 0) {
                    text = text.substr(6);
                    prefix = "☐ ";
                } else if (text.rfind("- ", 0) == 0) {
                    text = text.substr(2);
                    prefix = "• ";
                } else if (text.rfind("> ", 0) == 0) {
                    text = text.substr(2);
                    prefix = "│ ";
                    color = fg(139, 147, 167);
                }
                auto wrapped = wrap_display(prefix + text, dw - 4);
                for (const auto& line : wrapped) {
                    if (remaining-- <= 0) break;
                    add(std::string(C_DETAIL) + "  " + color + line);
                }
                if (remaining <= 0) break;
            }
        }
        if (!e->attachments.empty()) {
            add(std::string(C_DETAIL));
            for (const auto& att : e->attachments) {
                bool exists = repo_.store.attachment_exists(att);
                add(std::string(C_DETAIL) + " " + fg(45, 212, 191) + "⧉ " +
                    truncate_display(att.name, std::max(8, dw - 30)) + fg(90, 100, 125) +
                    "  " + human_size(att.size) +
                    (exists ? "" : fg(248, 113, 113) + " 文件丢失"));
                if ((int)detail.size() >= body_h - 1) break;
            }
        }
        while ((int)detail.size() < body_h) add(std::string(C_DETAIL));
    }

    void draw_help() {
        int w = std::max(80, console_.width() - 1);
        int h = std::max(20, console_.height() - 1);
        std::vector<std::string> text = {
            "墨记 · TUI 快捷键",
            "",
            "↑ ↓        选择任务",
            "Space / d  勾选 / 取消勾选（也支持鼠标点左侧方框）",
            "a          新增（标题、截止、标签、提醒、位置、备注）",
            "e          编辑选中任务",
            "x / Del    删除（需确认）",
            "[ / ]      提高 / 降低优先级",
            "t / 0 / 1 / 2 / 3   今日 / 全部 / 待办 / 已完成 / 逾期",
            "/          搜索；Esc 清空搜索",
            "o          查看附件",
            "Ctrl+Z/Y   撤销 / 重做",
            "E / I      导出 / 导入",
            "?          本帮助",
            "q          退出",
            "",
            "数据目录：" + repo_.display_dir(),
            "",
            "按任意键返回",
        };
        std::string out = "\x1b[H\x1b[2J";
        for (int i = 0; i < h; ++i) {
            std::string line;
            if (i < (int)text.size()) {
                int pad = (w - display_width(text[(size_t)i])) / 2;
                line = std::string((size_t)std::max(0, pad), ' ') + fg(230, 233, 239) +
                       text[(size_t)i];
            }
            out += line + "\x1b[K\r\n";
        }
        out += RESET;
        console_.write(out);
        console_.read_key(30000);
    }

    bool handle(const Key& key) {
        switch (key.type) {
            case Key::Type::Quit: return true;
            case Key::Type::Up: move_selection(-1); return false;
            case Key::Type::Down: move_selection(1); return false;
            case Key::Type::PageUp: move_selection(-5); return false;
            case Key::Type::PageDown: move_selection(5); return false;
            case Key::Type::Home:
                if (!visible_.empty()) selected_id_ = visible_.front()->id;
                return false;
            case Key::Type::End:
                if (!visible_.empty()) selected_id_ = visible_.back()->id;
                return false;
            case Key::Type::WheelUp: move_selection(-1); return false;
            case Key::Type::WheelDown: move_selection(1); return false;
            case Key::Type::Resize: return false;
            case Key::Type::Mouse: handle_mouse(key); return false;
            case Key::Type::Undo: do_undo(false); return false;
            case Key::Type::Redo: do_undo(true); return false;
            case Key::Type::Escape:
                search_.clear();
                refresh_visible(true);
                return false;
            case Key::Type::Delete: do_delete(); return false;
            default: break;
        }
        if (key.type == Key::Type::Char) {
            char c = (char)key.ch;
            switch (c) {
                case 'q': return true;
                case 'a': do_add(); return false;
                case 'e': do_edit(); return false;
                case ' ': case 'd': do_toggle(); return false;
                case 'x': do_delete(); return false;
                case '[': do_move(-1); return false;
                case ']': do_move(1); return false;
                case 't': set_mode("today"); return false;
                case '0': set_mode("all"); return false;
                case '1': set_mode("pending"); return false;
                case '2': set_mode("done"); return false;
                case '3': set_mode("overdue"); return false;
                case '/': do_search(); return false;
                case 'o': do_attachments(); return false;
                case 'E': do_export(); return false;
                case 'I': do_import(); return false;
                case '?': draw_help(); return false;
                case 'r':
                    repo_.reload();
                    refresh_visible(true);
                    sync_mtime();
                    toast("已刷新");
                    return false;
                default: break;
            }
        }
        return false;
    }

    void set_mode(const std::string& mode) {
        mode_ = mode;
        refresh_visible(true);
        toast("筛选：" + mode_name(), 1);
    }

    void handle_mouse(const Key& key) {
        if (key.x >= last_sidebar_w_ || key.y < last_list_top_) return;
        int rows = std::max(1, (console_.height() - 2 - 6 - 2) / 2);
        int rel = key.y - last_list_top_;
        int row = rel / 2;
        int idx = scroll_ + row;
        if (row < 0 || row >= rows || idx < 0 || idx >= (int)visible_.size()) return;
        Event* e = visible_[(size_t)idx];
        selected_id_ = e->id;
        if (key.x <= 4) do_toggle();
    }

    void do_toggle() {
        Event* e = current();
        if (!e) return;
        std::string id = e->id;
        try {
            Event& updated = repo_.toggle_done(id);
            toast(updated.done ? "已完成：" + updated.title : "恢复待办：" + updated.title, 2);
            refresh_visible();
            selected_id_ = id;
            sync_mtime();
        } catch (const std::exception& ex) {
            toast(std::string("操作失败：") + ex.what(), 4);
        }
    }

    void do_move(int delta) {
        Event* e = current();
        if (!e) return;
        std::string id = e->id;
        try {
            repo_.move_by(id, delta);
            refresh_visible();
            selected_id_ = id;
            Event* moved = repo_.find(id);
            toast("优先级 → P" + std::to_string(moved ? moved->priority : 0), 2);
            sync_mtime();
        } catch (const std::exception& ex) {
            toast(std::string("调整失败：") + ex.what(), 4);
        }
    }

    void do_delete() {
        Event* e = current();
        if (!e) return;
        std::string id = e->id;
        std::string title = e->title;
        console_.begin_input();
        std::string ans = console_.ask("删除「" + title + "」？输入 y 确认", "N");
        console_.end_input();
        if (ans != "y" && ans != "Y") {
            toast("已取消", 2);
            return;
        }
        repo_.remove(id);
        selected_id_.clear();
        refresh_visible(true);
        sync_mtime();
        toast("已删除（Ctrl+Z 可撤销）", 3);
    }

    void do_add() {
        console_.begin_input();
        std::cout << "—— 新建记事 ——\n";
        std::string title = trim(console_.ask("标题"));
        if (title.empty()) {
            console_.end_input();
            toast("已取消：标题为空", 2);
            return;
        }
        std::string due_text = console_.ask("截止时间 YYYY-MM-DD HH:MM（可空）");
        std::optional<std::time_t> due;
        while (!due_text.empty()) {
            due = parse_datetime(due_text);
            if (due.has_value()) break;
            due_text = console_.ask("时间格式不对，请重新输入（回车跳过）");
        }
        std::string tags_text = console_.ask("标签（逗号分隔，可空）");
        std::string remind_text = console_.ask("提前提醒分钟", "0");
        std::string pos_text = console_.ask("插入到第几位（可空=末尾）");
        std::string note = console_.ask("备注（一行，可空）");
        console_.end_input();

        int remind = std::atoi(remind_text.c_str());
        std::optional<int> pos;
        if (!pos_text.empty()) {
            int p = std::atoi(pos_text.c_str());
            if (p >= 1) pos = p;
        }
        try {
            Event& e = repo_.add(title, note, due, split_tags(tags_text), pos, remind);
            selected_id_ = e.id;
            mode_ = "all";
            search_.clear();
            refresh_visible();
            sync_mtime();
            toast("已加入 P" + std::to_string(e.priority) + "：" + e.title, 3);
        } catch (const std::exception& ex) {
            toast(std::string("新增失败：") + ex.what(), 5);
        }
    }

    void do_edit() {
        Event* e = current();
        if (!e) return;
        std::string id = e->id;
        Event snapshot = *e;
        console_.begin_input();
        std::cout << "—— 编辑记事 ——\n";
        std::cout << "当前标题：" << snapshot.title << "\n";
        if (!snapshot.note.empty()) std::cout << "当前备注：\n" << snapshot.note << "\n";
        std::string title = console_.ask("新标题（回车保持不变）", snapshot.title);
        std::string due_default =
            snapshot.due.has_value() ? iso_from_time(*snapshot.due) : "";
        std::string due_text = console_.ask("新截止（回车保持，- 清除）", due_default);
        std::string tags_text = console_.ask("新标签", join(snapshot.tags, ", "));
        std::string remind_text =
            console_.ask("提前提醒分钟", std::to_string(snapshot.remind_before));
        std::string pos_text =
            console_.ask("新的优先级位置（回车保持）", std::to_string(snapshot.priority));
        std::string note = console_.ask("新备注（回车保持，- 清除）", "");
        console_.end_input();

        std::optional<std::time_t> due = snapshot.due;
        if (due_text == "-") {
            due.reset();
        } else if (!due_text.empty()) {
            auto parsed = parse_datetime(due_text);
            if (!parsed.has_value()) {
                toast("时间格式不对，未保存", 5);
                return;
            }
            due = parsed;
        }
        std::string new_note = snapshot.note;
        if (note == "-") new_note.clear();
        else if (!note.empty()) new_note = note;
        std::vector<std::string> tags =
            trim(tags_text).empty() ? snapshot.tags : split_tags(tags_text);
        int remind = std::atoi(remind_text.c_str());
        try {
            repo_.update(id, title, new_note, due, tags, remind);
            int pos = std::atoi(pos_text.c_str());
            if (pos >= 1 && pos != snapshot.priority) repo_.move_to(id, pos);
            refresh_visible();
            selected_id_ = id;
            sync_mtime();
            toast("已保存修改", 2);
        } catch (const std::exception& ex) {
            toast(std::string("保存失败：") + ex.what(), 5);
        }
    }

    void do_search() {
        console_.begin_input();
        std::string text = console_.ask("搜索关键字（回车清空）", search_);
        console_.end_input();
        search_ = trim(text);
        mode_ = "all";
        refresh_visible(true);
        if (!search_.empty()) toast("搜索：" + search_, 2);
    }

    void do_attachments() {
        Event* e = current();
        if (!e) return;
        if (e->attachments.empty()) {
            toast("该记事没有附件（可在 GUI 中拖入）", 3);
            return;
        }
        console_.begin_input();
        std::cout << "—— 附件 ——\n";
        for (size_t i = 0; i < e->attachments.size(); ++i) {
            std::cout << "  " << (i + 1) << ". " << e->attachments[i].name << "  "
                      << human_size(e->attachments[i].size) << "\n";
        }
        std::string choice = console_.ask("输入编号打开（回车=1）", "1");
        console_.end_input();
        int idx = std::atoi(choice.c_str()) - 1;
        if (idx < 0 || idx >= (int)e->attachments.size()) {
            toast("编号无效", 2);
            return;
        }
        try {
            fs::path path = repo_.store.resolve(e->attachments[(size_t)idx]);
            std::wstring wpath = path.wstring();
            ShellExecuteW(nullptr, L"open", wpath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            toast("已打开 " + e->attachments[(size_t)idx].name, 2);
        } catch (const std::exception& ex) {
            toast(std::string("无法打开：") + ex.what(), 4);
        }
    }

    void do_export() {
        console_.begin_input();
        std::string path = console_.ask("导出到（.mjnote/.json/.txt/.md）");
        console_.end_input();
        if (path.empty()) {
            toast("已取消", 2);
            return;
        }
        try {
            export_auto(repo_, sub_path(path));
            toast("已导出到 " + path, 5);
            sync_mtime();
        } catch (const std::exception& ex) {
            toast(std::string("导出失败：") + ex.what(), 6);
        }
    }

    void do_import() {
        console_.begin_input();
        std::string path = console_.ask("从文件导入（.mjnote/.json/.txt）");
        std::string merge_text = console_.ask("合并导入？(Y/n)", "Y");
        console_.end_input();
        if (path.empty()) {
            toast("已取消", 2);
            return;
        }
        try {
            ImportReport r =
                import_auto(repo_, sub_path(path), merge_text != "n" && merge_text != "N");
            refresh_visible(true);
            sync_mtime();
            toast("导入完成：新增 " + std::to_string(r.added) + " 条，跳过 " +
                      std::to_string(r.skipped) + " 条",
                  5);
        } catch (const std::exception& ex) {
            toast(std::string("导入失败：") + ex.what(), 6);
        }
    }

    void do_undo(bool redo) {
        bool ok = redo ? repo_.redo() : repo_.undo();
        refresh_visible(true);
        sync_mtime();
        toast(redo ? (ok ? "已重做" : "没有可重做的操作")
                   : (ok ? "已撤销" : "没有可撤销的操作"),
              2);
    }

    static fs::path sub_path(const std::string& s) {
#ifdef _WIN32
        return fs::path(to_wide(s));
#else
        return fs::path(s);
#endif
    }
};

}  // namespace

int run_tui(Repository& repo) {
    Tui app(repo);
    return app.run();
}

#else  // POSIX 简易菜单 TUI

namespace {
int run_menu_tui(Repository& repo) {
    std::cout << "墨记 MoJi（POSIX 菜单模式）\n";
    for (;;) {
        std::cout << "\n1 显示 2 按日期 3 搜索 4 新增 5 完成/取消 6 删除 7 调序 8 统计 0 退出\n> ";
        std::string cmd;
        if (!std::getline(std::cin, cmd)) break;
        int c = std::atoi(cmd.c_str());
        if (c == 0) break;
        if (c == 1) {
            for (Event* e : repo.all()) {
                std::cout << "P" << e->priority << " " << (e->done ? "[x] " : "[ ] ")
                          << e->title << "  " << humanize_due(e->due) << "\n";
            }
        } else if (c == 4) {
            std::cout << "标题：";
            std::string title;
            std::getline(std::cin, title);
            if (!title.empty()) {
                try {
                    repo.add(title);
                } catch (const std::exception& ex) {
                    std::cout << ex.what() << "\n";
                }
            }
        } else if (c == 8) {
            Stats s = repo.stats();
            std::cout << "共 " << s.total << "，待办 " << s.pending << "，完成 " << s.done
                      << "，逾期 " << s.overdue << "\n";
        }
    }
    return 0;
}
}  // namespace

int run_tui(Repository& repo) { return run_menu_tui(repo); }

#endif

}  // namespace moji

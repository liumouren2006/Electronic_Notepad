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
#include "core/sound.hpp"
#include "core/util.hpp"
#include <nlohmann/json.hpp>

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

std::string repeat_text(const std::string& text, int count) {
    std::string out;
    for (int i = 0; i < std::max(0, count); ++i) out += text;
    return out;
}

bool parse_table_row(const std::string& line, std::vector<std::string>& cells) {
    std::string s = trim(line);
    if (s.size() < 2 || s.front() != '|') return false;
    s.erase(s.begin());
    if (!s.empty() && s.back() == '|') s.pop_back();
    cells.clear();
    std::string cur;
    for (char c : s) {
        if (c == '|') {
            cells.push_back(trim(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    cells.push_back(trim(cur));
    return !cells.empty();
}

bool table_separator(const std::vector<std::string>& cells) {
    if (cells.empty()) return false;
    for (const auto& cell : cells) {
        if (cell.empty()) return false;
        bool dash = false;
        for (char c : cell) {
            if (c == '-') dash = true;
            else if (c != ':' && c != ' ') return false;
        }
        if (!dash) return false;
    }
    return true;
}

std::string strip_color_tags(std::string s) {
    for (;;) {
        size_t open = s.find("{color:");
        if (open == std::string::npos) break;
        size_t close = s.find('}', open);
        if (close == std::string::npos) break;
        s.erase(open, close - open + 1);
    }
    for (;;) {
        size_t close = s.find("{/color}");
        if (close == std::string::npos) break;
        s.erase(close, 8);
    }
    return s;
}

std::string inline_plain(std::string s) {
    // 去掉常见的行内标记，终端里不显示 **、++ 之类源码符号
    for (const char* mark : {"**", "++", "~~", "=="}) {
        std::string m = mark;
        size_t pos = 0;
        while ((pos = s.find(m, pos)) != std::string::npos) s.erase(pos, m.size());
    }
    return trim(strip_color_tags(s));
}

std::string plain_cell(std::string s) { return inline_plain(s); }

struct Rgb {
    int r = 255, g = 255, b = 255;
};

struct TuiTheme {
    Rgb bg{11, 14, 20};
    Rgb panel{13, 17, 28};
    Rgb card{27, 36, 56};
    Rgb detail{11, 14, 20};
    Rgb bar{15, 19, 32};
    Rgb text{230, 233, 239};
    Rgb muted{139, 147, 167};
    Rgb dim{86, 96, 122};
    Rgb primary{124, 156, 255};
    Rgb success{74, 222, 128};
    Rgb warning{251, 191, 36};
    Rgb error{248, 113, 113};
    Rgb violet{167, 139, 250};
    Rgb teal{45, 212, 191};
    Rgb border{42, 49, 66};
    Rgb note_text{196, 201, 214};
    Rgb white{255, 255, 255};
    int sidebar_width = 44;
    std::string font_face;   // 留空表示不修改终端字体
    int font_size = 0;
};

std::string fg(const Rgb& c) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "\x1b[38;2;%d;%d;%dm", c.r, c.g, c.b);
    return buf;
}
std::string bg(const Rgb& c) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "\x1b[48;2;%d;%d;%dm", c.r, c.g, c.b);
    return buf;
}

Rgb parse_hex(const std::string& text, const Rgb& fallback) {
    std::string s = text;
    if (!s.empty() && s[0] == '#') s.erase(s.begin());
    if (s.size() != 6) return fallback;
    try {
        int v = std::stoi(s, nullptr, 16);
        return Rgb{(v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF};
    } catch (...) {
        return fallback;
    }
}

TuiTheme load_tui_theme(const fs::path& data_dir) {
    TuiTheme t;
    fs::path path = data_dir / "tui-theme.json";
    std::error_code ec;
    if (!fs::exists(path, ec)) return t;
    std::string text = read_file(path, nullptr);
    if (text.empty()) return t;
    try {
        auto j = nlohmann::json::parse(text);
        t.sidebar_width = std::max(30, std::min(60, j.value("sidebar_width", 44)));
        t.font_face = j.value("font_face", std::string(""));
        t.font_size = j.value("font_size", 0);
        if (j.contains("colors") && j["colors"].is_object()) {
            const auto& c = j["colors"];
            auto get = [&](const char* key, const Rgb& fallback) {
                return c.contains(key) && c[key].is_string()
                           ? parse_hex(c[key].get<std::string>(), fallback)
                           : fallback;
            };
            t.bg = get("bg", t.bg);
            t.panel = get("panel", t.panel);
            t.card = get("card", t.card);
            t.detail = get("detail", t.detail);
            t.bar = get("bar", t.bar);
            t.text = get("text", t.text);
            t.muted = get("muted", t.muted);
            t.dim = get("dim", t.dim);
            t.primary = get("primary", t.primary);
            t.success = get("success", t.success);
            t.warning = get("warning", t.warning);
            t.error = get("error", t.error);
            t.violet = get("violet", t.violet);
            t.teal = get("teal", t.teal);
            t.border = get("border", t.border);
            t.note_text = get("note", t.note_text);
        }
    } catch (...) {
    }
    return t;
}

struct Key {
    enum class Type {
        None,
        Char,
        Up,
        Down,
        Enter,
        Escape,
        Backspace,
        Delete,
        PageUp,
        PageDown,
        Home,
        End,
        Undo,
        Redo,
        CtrlS,
        Function,
        Mouse,
        WheelUp,
        WheelDown,
        Resize,
        Quit
    } type = Type::None;
    wchar_t ch = 0;
    int x = 0;
    int y = 0;
    int fkey = 0;
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

    void set_font(const std::string& face, int size) {
        if (face.empty() || size <= 0) return;
        CONSOLE_FONT_INFOEX info{};
        info.cbSize = sizeof(info);
        if (!GetCurrentConsoleFontEx(out_, FALSE, &info)) return;
        std::wstring wide = to_wide(face);
        wcsncpy(info.FaceName, wide.c_str(), LF_FACESIZE - 1);
        info.dwFontSize.X = 0;
        info.dwFontSize.Y = (SHORT)size;
        SetCurrentConsoleFontEx(out_, FALSE, &info);
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
                if (k.wVirtualKeyCode == 'S') return Key{Key::Type::CtrlS};
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
                case VK_BACK: out.type = Key::Type::Backspace; return out;
                case VK_DELETE: out.type = Key::Type::Delete; return out;
                case VK_F1: case VK_F2: case VK_F3: case VK_F4:
                    out.type = Key::Type::Function;
                    out.fkey = k.wVirtualKeyCode - VK_F1 + 1;
                    return out;
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
    explicit Tui(Repository& repo) : repo_(repo) {
        theme_ = load_tui_theme(repo_.store.dir);
        ringtone_ = repo_.store.load_config().value("ringtone", std::string(""));
    }

    int run() {
        if (!console_.init()) {
            std::cerr << "无法初始化控制台。\n";
            return 2;
        }
        console_.set_font(theme_.font_face, theme_.font_size);
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
    TuiTheme theme_;
    std::string ringtone_;
    std::string mode_ = "all";
    std::string search_;
    bool search_editing_ = false;
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
            if (ringtone_.empty() || !play_ringtone(ringtone_)) {
                console_.beep();
            }
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
        int sw = theme_.sidebar_width;
        if (w < 90) sw = 34;
        int dw = w - sw - 1;
        last_sidebar_w_ = sw;
        int body_h = h - 2;
        last_list_top_ = 6;

        Stats stats = repo_.stats();
        std::vector<std::string> lines;

        std::string left = bg(theme_.bar) + " " + fg(theme_.text) + "◉ 墨记 MoJi";
        std::string center = "— 电子记事本 · TUI";
        std::string right = clock_str(now_time()) + " ";
        int left_w = visible_width(left);
        int center_w = display_width(center);
        int right_w = visible_width(right);
        int center_x = std::max(left_w + 1, (w - center_w) / 2);
        int pad1 = std::max(1, center_x - left_w);
        int pad2 = std::max(1, w - center_x - center_w - right_w);
        std::string header = left + std::string((size_t)pad1, ' ') +
                             fg(theme_.muted) + center +
                             std::string((size_t)pad2, ' ') + fg(theme_.muted) + right;
        lines.push_back(pad_line(header, w) + RESET);

        std::vector<std::string> side;
        std::vector<std::string> detail;
        build_sidebar(side, sw, stats);
        build_detail(detail, dw, body_h);

        for (int i = 0; i < body_h; ++i) {
            std::string s = i < (int)side.size() ? side[(size_t)i] : bg(theme_.panel);
            std::string d = i < (int)detail.size() ? detail[(size_t)i] : bg(theme_.detail);
            std::string line = pad_line(s, sw) + fg(theme_.border) + "│" + pad_line(d, dw);
            lines.push_back(line + RESET);
        }

        std::string footer_plain =
            search_editing_
                ? "搜索模式：直接输入　Enter 确认　Backspace 删除　Esc 清空"
                : "↑↓ 选择　Space 勾选　a 新增　e 编辑　x 删除　[ ] 调序　t 今日　/ 搜索　E 导出　? 帮助　q 退出";
        if (display_width(footer_plain) > w - 2) {
            footer_plain = truncate_display(footer_plain, w - 2);
        }
        lines.push_back(pad_line(bg(theme_.bar) + " " + fg(theme_.muted) + footer_plain,
                                 w) +
                        RESET);

        std::string out = "\x1b[H";
        for (const auto& line : lines) out += line + "\x1b[K\r\n";
        out += "\x1b[J";
        console_.write(out);
    }

    void build_sidebar(std::vector<std::string>& side, int sw, const Stats& stats) {
        auto add = [&](const std::string& line) { side.push_back(pad_line(line, sw)); };
        add(bg(theme_.panel) + " " + fg(theme_.muted) + "今日 " + fg(theme_.primary) +
            std::to_string(stats.today) + "　　" + fg(theme_.muted) + "逾期 " +
            fg(stats.overdue ? theme_.error : theme_.muted) + std::to_string(stats.overdue));
        add(bg(theme_.panel) + " " + fg(theme_.muted) + "待办 " + fg(theme_.text) +
            std::to_string(stats.pending) + "　　" + fg(theme_.muted) + "完成 " +
            fg(theme_.success) + std::to_string(stats.rate) + "%");
        int box_w = std::max(20, sw);
        int box_inner = box_w - 4;
        Rgb border_color = search_editing_ ? theme_.primary : theme_.border;
        std::string top = bg(theme_.panel) + fg(border_color) + "╭" +
                          repeat_text("─", box_w - 2) + "╮";
        std::string shown;
        std::string shown_color;
        if (!search_.empty()) {
            shown = "🔍 " + search_;
            shown_color = fg(theme_.text);
        } else {
            shown = "🔍 搜索标题、标签、正文…";
            shown_color = fg(theme_.dim);
        }
        if (search_editing_) {
            shown += "▏";
            shown_color = fg(theme_.text);
        }
        shown = truncate_display(shown, box_inner);
        std::string mid = bg(theme_.panel) + fg(border_color) + "│ " + shown_color +
                          pad_display(shown, box_inner) + fg(border_color) + " │";
        std::string bottom = bg(theme_.panel) + fg(border_color) + "╰" +
                             repeat_text("─", box_w - 2) + "╯";
        add(top);
        add(mid);
        add(bottom);
        add(bg(theme_.panel) + " " + fg(theme_.primary) + "◈ " + mode_name() +
            fg(theme_.dim) + "  共 " + std::to_string(visible_.size()) + " 条");
        add(bg(theme_.panel));

        int idx = current_index();
        last_list_top_ = (int)side.size() + 1;
        int rows = std::max(1, (console_.height() - 2 - last_list_top_ - 2) / 2);
        if (idx >= 0) {
            if (idx < scroll_) scroll_ = idx;
            if (idx >= scroll_ + rows) scroll_ = idx - rows + 1;
        }
        scroll_ = std::max(0, std::min(scroll_, std::max(0, (int)visible_.size() - 1)));

        if (visible_.empty()) {
            add(bg(theme_.panel) + " " + fg(theme_.dim) + "没有符合条件的记事");
        }
        for (int i = scroll_; i < (int)visible_.size() && i < scroll_ + rows; ++i) {
            Event* e = visible_[(size_t)i];
            bool sel = (i == idx);
            std::string row_bg = sel ? bg(theme_.card) : bg(theme_.panel);
            std::string mark = e->done ? "✔" : "◻";
            std::string mark_color =
                e->done ? fg(theme_.success) : fg(theme_.dim);
            std::string title_color =
                e->done ? fg(theme_.dim) : (sel ? fg(theme_.white) : fg(theme_.text));
            std::string line1 = std::string(row_bg) + " " + mark_color + mark + " " +
                                title_color + truncate_display(e->title, sw - 10) +
                                " " + fg(theme_.primary) + "P" + std::to_string(e->priority);
            add(line1);
            std::string due_color;
            if (e->done) due_color = fg(theme_.dim);
            else if (e->is_overdue()) due_color = fg(theme_.error);
            else if (e->is_due_today()) due_color = fg(theme_.warning);
            else due_color = fg(theme_.muted);
            std::string line2 = std::string(row_bg) + "   " + due_color + humanize_due(e->due);
            for (size_t t = 0; t < e->tags.size() && t < 2; ++t) {
                line2 += " " + fg(theme_.violet) + "#" + truncate_display(e->tags[t], 8);
            }
            if (!e->attachments.empty()) {
                line2 += " " + fg(theme_.teal) + "⧉" + std::to_string(e->attachments.size());
            }
            add(line2);
        }
        while ((int)side.size() < console_.height() - 2) add(bg(theme_.panel));
    }

    void build_detail(std::vector<std::string>& detail, int dw, int body_h) {
        auto add = [&](const std::string& line) { detail.push_back(pad_line(line, dw)); };
        Event* e = current();
        if (!e) {
            add(bg(theme_.detail) + " " + fg(theme_.dim) + "未选择记事");
            add(bg(theme_.detail));
            add(bg(theme_.detail) + " " + fg(theme_.muted) +
                "按 a 新建，或从左侧选择一条记事。");
            while ((int)detail.size() < body_h) add(bg(theme_.detail));
            return;
        }
        std::string mark = e->done ? "✔" : "○";
        std::string title_color = e->done ? fg(theme_.success) : fg(theme_.white);
        add(bg(theme_.detail) + " " + title_color + mark + " " + fg(theme_.text) +
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
        add(bg(theme_.detail) + " " + fg(theme_.muted) +
            truncate_display(meta_plain, dw - 3));
        std::string sep = bg(theme_.detail) + " " + fg(theme_.border);
        for (int i = 0; i < dw - 2; ++i) sep += "─";
        add(sep);

        if (!toast_.empty()) {
            add(bg(theme_.detail) + " " + fg(theme_.warning) +
                truncate_display(toast_, dw - 3));
            add(bg(theme_.detail));
        }
        int remaining = body_h - (int)detail.size() - 2;
        auto render_table = [&](const std::vector<std::vector<std::string>>& rows) {
            if (rows.empty() || remaining < 3) return;
            size_t ncols = 1;
            for (const auto& row : rows) ncols = std::max(ncols, row.size());
            std::vector<int> widths(ncols, 4);
            for (const auto& row : rows) {
                for (size_t c = 0; c < row.size(); ++c) {
                    widths[c] = std::max(widths[c], display_width(plain_cell(row[c])) + 2);
                }
            }
            int max_width = std::max(18, dw - 4);
            int total = (int)ncols + 1;
            for (int w : widths) total += w;
            while (total > max_width) {
                size_t widest = 0;
                for (size_t c = 1; c < widths.size(); ++c) {
                    if (widths[c] > widths[widest]) widest = c;
                }
                if (widths[widest] <= 4) break;
                --widths[widest];
                --total;
            }
            auto border_line = [&](const char* left, const char* middle, const char* right) {
                std::string s = bg(theme_.detail) + "  " + fg(theme_.border) + left;
                for (size_t c = 0; c < widths.size(); ++c) {
                    s += repeat_text("─", widths[c]);
                    if (c + 1 < widths.size()) s += middle;
                }
                s += right;
                return s;
            };
            auto emit_row = [&](const std::vector<std::string>& row, bool header) {
                if (remaining-- <= 0) return;
                std::string s = bg(theme_.detail) + "  " + fg(theme_.border) + "│";
                for (size_t c = 0; c < widths.size(); ++c) {
                    std::string cell = c < row.size() ? plain_cell(row[c]) : "";
                    int inner = std::max(1, widths[c] - 2);
                    s += fg(header ? theme_.white : theme_.note_text) + " " +
                         pad_display(truncate_display(cell, inner), inner) + " " +
                         fg(theme_.border) + "│";
                }
                add(s);
            };
            add(border_line("┌", "┬", "┐"));
            emit_row(rows[0], true);
            if (remaining <= 0) return;
            add(border_line("├", "┼", "┤"));
            for (size_t r = 1; r < rows.size() && remaining > 0; ++r) {
                emit_row(rows[r], false);
            }
            if (remaining > 0) add(border_line("└", "┴", "┘"));
        };

        if (e->note.empty()) {
            add(bg(theme_.detail) + " " + fg(theme_.dim) + "暂无备注，按 e 编辑");
        } else {
            auto note_lines = split(e->note, '\n');
            for (size_t i = 0; i < note_lines.size();) {
                std::vector<std::string> cells;
                std::vector<std::string> next_cells;
                if (parse_table_row(note_lines[i], cells) && i + 1 < note_lines.size() &&
                    parse_table_row(note_lines[i + 1], next_cells) &&
                    table_separator(next_cells)) {
                    std::vector<std::vector<std::string>> table_rows;
                    table_rows.push_back(cells);
                    i += 2;
                    while (i < note_lines.size()) {
                        std::vector<std::string> body;
                        if (!parse_table_row(note_lines[i], body) || table_separator(body)) break;
                        table_rows.push_back(body);
                        ++i;
                    }
                    render_table(table_rows);
                    if (remaining <= 0) break;
                    continue;
                }

                const std::string& raw = note_lines[i++];
                std::string text = inline_plain(raw);
                std::string prefix;
                std::string color = fg(theme_.note_text);
                if (text.rfind("### ", 0) == 0) {
                    text = text.substr(4);
                    color = fg(theme_.text);
                } else if (text.rfind("## ", 0) == 0) {
                    text = text.substr(3);
                    color = fg(theme_.text);
                } else if (text.rfind("# ", 0) == 0) {
                    text = text.substr(2);
                    color = fg(theme_.text);
                } else if (text.rfind("- [x] ", 0) == 0) {
                    text = text.substr(6);
                    prefix = "☑ ";
                    color = fg(theme_.muted);
                } else if (text.rfind("- [ ] ", 0) == 0) {
                    text = text.substr(6);
                    prefix = "☐ ";
                } else if (text.rfind("- ", 0) == 0) {
                    text = text.substr(2);
                    prefix = "• ";
                } else if (text.rfind("> ", 0) == 0) {
                    text = text.substr(2);
                    prefix = "│ ";
                    color = fg(theme_.muted);
                }
                auto wrapped = wrap_display(prefix + text, dw - 4);
                for (const auto& line : wrapped) {
                    if (remaining-- <= 0) break;
                    add(bg(theme_.detail) + "  " + color + line);
                }
                if (remaining <= 0) break;
            }
        }
        if (!e->attachments.empty()) {
            add(bg(theme_.detail));
            for (const auto& att : e->attachments) {
                bool exists = repo_.store.attachment_exists(att);
                add(bg(theme_.detail) + " " + fg(theme_.teal) + "⧉ " +
                    truncate_display(att.name, std::max(8, dw - 30)) + fg(theme_.dim) +
                    "  " + human_size(att.size) +
                    (exists ? "" : fg(theme_.error) + " 文件丢失"));
                if ((int)detail.size() >= body_h - 1) break;
            }
        }
        while ((int)detail.size() < body_h) add(bg(theme_.detail));
    }

    void overlay_box(const std::vector<std::string>& lines, int box_w) {
        int w = std::max(80, console_.width() - 1);
        int h = std::max(20, console_.height() - 1);
        int x = std::max(0, (w - box_w) / 2);
        int y = std::max(0, (h - (int)lines.size()) / 2);
        std::string out;
        for (size_t i = 0; i < lines.size(); ++i) {
            int row = y + (int)i + 1;
            // 先把整行铺成暗色再画弹窗：避免底下的宽字符跨到弹窗左边界，
            // 也形成类似 Python 版的压暗背景。
            out += "\x1b[" + std::to_string(row) + ";1H" + bg(theme_.bg) +
                   std::string((size_t)w, ' ');
            out += "\x1b[" + std::to_string(row) + ";" + std::to_string(x + 1) + "H" +
                   pad_line(lines[i], box_w) + bg(theme_.bg);
        }
        out += RESET;
        console_.write(out);
    }

    void draw_help() {
        draw();
        int w = std::max(80, console_.width() - 1);
        int box_w = std::min(86, std::max(60, w - 12));
        int inner = box_w - 4;
        auto box_row = [&](const std::string& content) {
            int cw = visible_width(content);
            std::string s = bg(theme_.panel) + fg(theme_.border) + "│" +
                            bg(theme_.panel) + " " + content + bg(theme_.panel) +
                            std::string((size_t)std::max(0, inner - cw), ' ') +
                            fg(theme_.border) + " │";
            return s;
        };
        auto border_line = [&](const char* left, const char* middle, const char* right,
                               int left_w, int right_w) {
            return bg(theme_.panel) + fg(theme_.border) + left +
                   repeat_text("─", left_w) + middle + repeat_text("─", right_w) + right;
        };
        int key_w = 22;
        int desc_w = std::max(20, inner - key_w - 3);
        auto table_row = [&](const std::string& key, const std::string& desc, bool header) {
            std::string content = fg(theme_.border) + "│ " +
                                  fg(header ? theme_.primary : theme_.warning) +
                                  pad_display(truncate_display(key, key_w - 2), key_w - 2) +
                                  fg(theme_.border) + " │ " +
                                  fg(header ? theme_.primary : theme_.note_text) +
                                  pad_display(truncate_display(desc, desc_w - 2), desc_w - 2) +
                                  fg(theme_.border) + " │";
            return box_row(content);
        };
        std::vector<std::string> box;
        box.push_back(bg(theme_.panel) + fg(theme_.primary) + "╭" +
                      repeat_text("─", box_w - 2) + "╮");
        box.push_back(box_row(fg(theme_.white) + "? 使用帮助"));
        box.push_back(box_row(""));
        std::string centered = "墨记 · 快捷键";
        int left_pad = std::max(0, (inner - display_width(centered)) / 2);
        box.push_back(box_row(std::string((size_t)left_pad, ' ') + fg(theme_.primary) +
                              centered));
        box.push_back(box_row(fg(theme_.primary) + "浏览"));
        box.push_back(box_row(border_line("┌", "┬", "┐", key_w, desc_w)));
        box.push_back(table_row("按键", "作用", true));
        box.push_back(box_row(border_line("├", "┼", "┤", key_w, desc_w)));
        struct HelpRow {
            const char* key;
            const char* desc;
        };
        const HelpRow rows[] = {
            {"↑ ↓ / k j", "上下选择"},
            {"Enter / e", "编辑选中事件"},
            {"Space / d", "勾选 / 取消勾选"},
            {"Delete / x", "删除（先确认）"},
            {"[ / ]", "提高 / 降低优先级"},
            {"t / 0 / 1 / 2 / 3", "今日 / 全部 / 待办 / 已完成 / 逾期"},
            {"/", "搜索（输入框内直接打字）"},
            {"o", "附件弹窗"},
            {"Ctrl+Z / Ctrl+Y", "撤销 / 重做"},
            {"E / I", "导出 / 导入"},
            {"?", "本帮助"},
            {"q", "退出"},
        };
        for (const auto& row : rows) box.push_back(table_row(row.key, row.desc, false));
        box.push_back(box_row(border_line("└", "┴", "┘", key_w, desc_w)));
        box.push_back(box_row(""));
        std::string close = "关闭 (Esc)";
        int close_pad = std::max(0, inner - display_width(close) - 4);
        box.push_back(box_row(std::string((size_t)close_pad, ' ') + bg(theme_.primary) +
                              fg(theme_.white) + " " + close + " "));
        box.push_back(bg(theme_.panel) + fg(theme_.primary) + "╰" +
                      repeat_text("─", box_w - 2) + "╯");
        overlay_box(box, box_w);
        console_.read_key(60000);
    }

    bool handle(const Key& key) {
        if (search_editing_) {
            if (key.type == Key::Type::Escape) {
                search_.clear();
                search_editing_ = false;
                refresh_visible(true);
                toast("已清空搜索", 2);
                return false;
            }
            if (key.type == Key::Type::Enter) {
                search_ = trim(search_);
                search_editing_ = false;
                mode_ = "all";
                refresh_visible(true);
                if (!search_.empty()) toast("搜索：" + search_, 2);
                return false;
            }
            if (key.type == Key::Type::Backspace) {
                if (!search_.empty()) {
                    size_t pos = search_.size() - 1;
                    while (pos > 0 && ((unsigned char)search_[pos] & 0xC0) == 0x80) --pos;
                    search_.erase(pos);
                    refresh_visible(true);
                }
                return false;
            }
            if (key.type == Key::Type::Char && key.ch >= 32) {
                std::wstring one(1, key.ch);
                search_ += to_utf8(one);
                refresh_visible(true);
                return false;
            }
            return false;
        }
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
                case '/':
                    search_editing_ = true;
                    toast("搜索：输入关键字，Enter 确认，Esc 清空", 3);
                    return false;
                case 'o': do_attachments(); return false;
                case 'E': do_export(); return false;
                case 'I': do_import(); return false;
                case '?': draw_help(); return false;
                case 'T':
                    theme_ = load_tui_theme(repo_.store.dir);
                    ringtone_ = repo_.store.load_config().value("ringtone", std::string(""));
                    console_.set_font(theme_.font_face, theme_.font_size);
                    toast("TUI 主题已重新加载");
                    return false;
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
        int rows = std::max(1, (console_.height() - 2 - last_list_top_ - 2) / 2);
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

    void show_event_form(Event* existing) {
        bool editing = existing != nullptr;
        std::string id = existing ? existing->id : "";
        int original_priority = existing ? existing->priority : 0;
        std::vector<std::string> values = {
            existing ? existing->title : "",
            existing && existing->due.has_value()
                ? date_str(*existing->due) + " " + clock_str(*existing->due)
                : "",
            existing ? std::to_string(existing->remind_before) : "0",
            existing ? std::to_string(existing->priority) : "",
            existing ? join(existing->tags, ", ") : "",
            existing ? existing->note : "",
        };
        int focus = 0;
        std::string error;
        auto fmt_time = [](std::time_t t) { return date_str(t) + " " + clock_str(t); };
        auto to_int = [](const std::string& text, int& out) {
            std::string t = trim(text);
            if (t.empty()) return false;
            size_t used = 0;
            try {
                out = std::stoi(t, &used);
            } catch (...) {
                return false;
            }
            return used == t.size();
        };

        for (;;) {
            draw();
            int w = std::max(80, console_.width() - 1);
            int h = std::max(24, console_.height() - 1);
            int box_w = std::min(94, std::max(62, w - 8));
            int inner = box_w - 4;
            int note_h = std::max(3, std::min(6, h - 20));

            auto row = [&](const std::string& content) {
                int cw = visible_width(content);
                return bg(theme_.panel) + fg(theme_.border) + "│" +
                       bg(theme_.panel) + " " + content + bg(theme_.panel) +
                       std::string((size_t)std::max(0, inner - cw), ' ') +
                       fg(theme_.border) + " │";
            };
            auto field = [&](const std::string& label, const std::string& value,
                             const std::string& placeholder, int width, bool focused) {
                int avail = std::max(2, width - 7);
                std::string shown = value.empty() ? placeholder : value;
                if (focused) shown += "▏";
                shown = truncate_display(shown, avail);
                std::string label_color = focused ? fg(theme_.primary) : fg(theme_.muted);
                std::string border_color = focused ? fg(theme_.primary) : fg(theme_.border);
                std::string text_color =
                    value.empty() && !focused ? fg(theme_.dim) : fg(theme_.text);
                std::string out = label_color + label + " " + border_color + "[" +
                                  text_color + shown;
                int pad = avail - display_width(shown);
                if (pad > 0) out += std::string((size_t)pad, ' ');
                out += border_color + "]";
                int used = visible_width(out);
                if (used < width) out += std::string((size_t)(width - used), ' ');
                return out;
            };

            std::vector<std::string> box;
            box.push_back(bg(theme_.panel) + fg(theme_.primary) + "╭" +
                          repeat_text("─", box_w - 2) + "╮");
            box.push_back(row(fg(theme_.white) +
                              (editing ? "✎ 编辑记事" : "＋ 新建记事")));
            box.push_back(row(""));
            box.push_back(row(field("标题", values[0], "要做什么？", inner - 2, focus == 0)));
            std::string due_row = field("截止", values[1], "YYYY-MM-DD HH:MM",
                                        38, focus == 1) +
                                  " " +
                                  field("提醒", values[2], "0", 14, focus == 2) +
                                  "分 " +
                                  field("位置", values[3], "末位", 12, focus == 3);
            box.push_back(row(due_row));
            box.push_back(row(fg(theme_.dim) + "快捷  " + fg(theme_.primary) + "F1" +
                              fg(theme_.muted) + " 今天18:00   " +
                              fg(theme_.primary) + "F2" + fg(theme_.muted) +
                              " 明天09:00   " + fg(theme_.primary) + "F3" +
                              fg(theme_.muted) + " 下周一09:00   " +
                              fg(theme_.primary) + "F4" + fg(theme_.muted) +
                              " 清除时间"));
            box.push_back(row(field("标签", values[4], "课程, 截止", inner - 4,
                                    focus == 4)));
            box.push_back(row(""));
            box.push_back(row(fg(theme_.muted) +
                              "备注（支持 Markdown；Enter 换行，Ctrl+S 保存）"));

            int nw = inner;
            std::string note_border =
                focus == 5 ? fg(theme_.primary) : fg(theme_.border);
            box.push_back(row(note_border + "┌" + repeat_text("─", nw - 2) + "┐"));
            auto note_lines = split(values[5], '\n');
            if (note_lines.empty()) note_lines.push_back("");
            int start = std::max(0, (int)note_lines.size() - note_h);
            for (int i = 0; i < note_h; ++i) {
                int index = start + i;
                std::string text =
                    index < (int)note_lines.size() ? note_lines[(size_t)index] : "";
                bool caret = focus == 5 && index == (int)note_lines.size() - 1;
                bool placeholder = text.empty() && focus != 5 && i == 0;
                std::string shown = placeholder ? "支持 Markdown…" : text;
                if (caret) shown += "▏";
                shown = truncate_display(shown, nw - 4);
                std::string color =
                    placeholder ? fg(theme_.dim) : fg(theme_.note_text);
                box.push_back(row(note_border + "│ " + color +
                                  pad_display(shown, nw - 4) + note_border + " │"));
            }
            box.push_back(row(note_border + "└" + repeat_text("─", nw - 2) + "┘"));
            box.push_back(row(""));
            box.push_back(row(error.empty()
                                  ? fg(theme_.dim) +
                                        "Tab / ↑↓ 切换 · Ctrl+S 保存 · Esc 取消"
                                  : fg(theme_.error) + truncate_display(error, inner)));
            std::string buttons =
                fg(theme_.muted) + "取消 (Esc)" + "      " + bg(theme_.primary) +
                fg(theme_.white) + " 保存 (Ctrl+S) ";
            int pad = std::max(0, inner - visible_width(buttons));
            box.push_back(row(std::string((size_t)pad, ' ') + buttons));
            box.push_back(bg(theme_.panel) + fg(theme_.primary) + "╰" +
                          repeat_text("─", box_w - 2) + "╯");
            overlay_box(box, box_w);

            Key key = console_.read_key(60000);
            if (key.type == Key::Type::None || key.type == Key::Type::Resize) continue;
            if (key.type == Key::Type::Escape) {
                toast("已取消编辑", 2);
                return;
            }
            if (key.type == Key::Type::Function) {
                std::time_t now = now_time();
                std::time_t day = start_of_day(now);
                if (key.fkey == 1) {
                    values[1] = fmt_time(day + 18 * 3600);
                } else if (key.fkey == 2) {
                    values[1] = fmt_time(start_of_day(now + 86400) + 9 * 3600);
                } else if (key.fkey == 3) {
                    std::tm tm{};
#ifdef _WIN32
                    localtime_s(&tm, &now);
#else
                    localtime_r(&now, &tm);
#endif
                    int days = (1 - tm.tm_wday + 7) % 7;
                    if (days == 0) days = 7;
                    values[1] = fmt_time(start_of_day(now) + days * 86400 + 9 * 3600);
                } else if (key.fkey == 4) {
                    values[1].clear();
                }
                focus = 1;
                error.clear();
                continue;
            }
            if (key.type == Key::Type::Up) {
                focus = (focus + 5) % 6;
                continue;
            }
            if (key.type == Key::Type::Down) {
                focus = (focus + 1) % 6;
                continue;
            }
            if (key.type == Key::Type::Char && key.ch == 9) {
                focus = (focus + 1) % 6;
                continue;
            }
            if (key.type == Key::Type::Backspace) {
                std::string& s = values[(size_t)focus];
                if (!s.empty()) {
                    size_t pos = s.size() - 1;
                    while (pos > 0 && ((unsigned char)s[pos] & 0xC0) == 0x80) --pos;
                    s.erase(pos);
                    error.clear();
                }
                continue;
            }
            if (key.type == Key::Type::Enter) {
                if (focus == 5) {
                    values[5] += "\n";
                } else {
                    focus = std::min(5, focus + 1);
                }
                continue;
            }
            if (key.type == Key::Type::CtrlS) {
                std::string title = trim(values[0]);
                if (title.empty()) {
                    error = "标题不能为空";
                    focus = 0;
                    continue;
                }
                std::string due_text = trim(values[1]);
                std::optional<std::time_t> due;
                if (!due_text.empty()) {
                    due = parse_datetime(due_text);
                    if (!due.has_value()) {
                        error = "截止时间格式不对，应为 YYYY-MM-DD HH:MM";
                        focus = 1;
                        continue;
                    }
                }
                int remind = 0;
                if (!trim(values[2]).empty() && !to_int(values[2], remind)) {
                    error = "提前提醒需要是非负整数（分钟）";
                    focus = 2;
                    continue;
                }
                if (remind < 0) {
                    error = "提前提醒需要是非负整数（分钟）";
                    focus = 2;
                    continue;
                }
                std::optional<int> pos;
                int pos_value = 0;
                if (!trim(values[3]).empty()) {
                    if (!to_int(values[3], pos_value) || pos_value < 1) {
                        error = "优先级位置需要是 ≥1 的整数";
                        focus = 3;
                        continue;
                    }
                    pos = pos_value;
                }
                std::vector<std::string> tags = split_tags(values[4]);
                try {
                    if (editing) {
                        repo_.update(id, title, values[5], due, tags, remind);
                        if (pos.has_value() && *pos != original_priority) {
                            repo_.move_to(id, *pos);
                        }
                        selected_id_ = id;
                        refresh_visible();
                        sync_mtime();
                        toast("已保存修改", 2);
                    } else {
                        Event& added =
                            repo_.add(title, values[5], due, tags, pos, remind);
                        selected_id_ = added.id;
                        mode_ = "all";
                        search_.clear();
                        refresh_visible();
                        sync_mtime();
                        toast("已加入 P" + std::to_string(added.priority) + "：" +
                                  added.title,
                              3);
                    }
                } catch (const std::exception& ex) {
                    error = std::string("保存失败：") + ex.what();
                }
                if (!error.empty()) continue;
                return;
            }
            if (key.type == Key::Type::Char && key.ch >= 32) {
                values[(size_t)focus] += to_utf8(std::wstring(1, key.ch));
                error.clear();
            }
        }
    }

    void do_add() { show_event_form(nullptr); }

    void do_edit() {
        Event* e = current();
        if (e) show_event_form(e);
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

    void draw_attachment_modal(const std::string& id, int selected) {
        draw();
        Event* e = repo_.find(id);
        if (!e) return;
        int w = std::max(80, console_.width() - 1);
        int box_w = std::min(74, std::max(52, w - 14));
        int inner = box_w - 4;
        auto box_row = [&](const std::string& content) {
            int cw = visible_width(content);
            return bg(theme_.panel) + fg(theme_.border) + "│" + bg(theme_.panel) + " " +
                   content + bg(theme_.panel) +
                   std::string((size_t)std::max(0, inner - cw), ' ') + fg(theme_.border) +
                   " │";
        };
        std::vector<std::string> box;
        box.push_back(bg(theme_.panel) + fg(theme_.primary) + "╭" +
                      repeat_text("─", box_w - 2) + "╮");
        std::string title_plain = "📎 附件 · " + e->title;
        std::string count_plain = "共 " + std::to_string(e->attachments.size()) +
                                  " 个 · ↑↓ 选择 · Enter 打开 · Del 移除 · Esc 关闭";
        box.push_back(box_row(fg(theme_.primary) +
                              truncate_display(title_plain, inner)));
        box.push_back(box_row(fg(theme_.dim) +
                              truncate_display(count_plain, inner)));
        int visible = std::min<int>(8, (int)e->attachments.size());
        int start = std::max(0, std::min(selected - visible / 2,
                                         (int)e->attachments.size() - visible));
        for (int i = start; i < start + visible; ++i) {
            const Attachment& att = e->attachments[(size_t)i];
            std::string text = " " + att.name + "  " + human_size(att.size) + " · " +
                               att.mime + " ";
            text = truncate_display(text, inner);
            if (i == selected) {
                text = pad_display(text, inner);
                box.push_back(box_row(bg(theme_.primary) + fg(theme_.white) + text));
            } else {
                box.push_back(box_row(fg(theme_.note_text) + text));
            }
        }
        box.push_back(box_row(""));
        std::string buttons = fg(theme_.muted) + "关闭" + "    " + fg(theme_.error) +
                              "移除" + "    " + fg(theme_.primary) + "打开";
        int pad = std::max(0, (inner - visible_width(buttons)) / 2);
        box.push_back(box_row(std::string((size_t)pad, ' ') + buttons));
        box.push_back(bg(theme_.panel) + fg(theme_.primary) + "╰" +
                      repeat_text("─", box_w - 2) + "╯");
        overlay_box(box, box_w);
    }

    void do_attachments() {
        Event* e = current();
        if (!e) return;
        if (e->attachments.empty()) {
            toast("该记事没有附件（可在 GUI 中拖入）", 3);
            return;
        }
        std::string id = e->id;
        int selected = 0;
        for (;;) {
            e = repo_.find(id);
            if (!e || e->attachments.empty()) break;
            selected = std::max(0, std::min(selected, (int)e->attachments.size() - 1));
            draw_attachment_modal(id, selected);
            Key key = console_.read_key(60000);
            if (key.type == Key::Type::Escape) break;
            if (key.type == Key::Type::Up) {
                selected = std::max(0, selected - 1);
                continue;
            }
            if (key.type == Key::Type::Down) {
                selected = std::min((int)e->attachments.size() - 1, selected + 1);
                continue;
            }
            bool open = key.type == Key::Type::Enter ||
                        (key.type == Key::Type::Char && key.ch == 'o');
            bool remove = key.type == Key::Type::Delete ||
                          (key.type == Key::Type::Char && key.ch == 'x');
            if (open) {
                try {
                    fs::path path = repo_.store.resolve(e->attachments[(size_t)selected]);
                    std::wstring wpath = path.wstring();
                    ShellExecuteW(nullptr, L"open", wpath.c_str(), nullptr, nullptr,
                                  SW_SHOWNORMAL);
                    toast("已打开 " + e->attachments[(size_t)selected].name, 2);
                } catch (const std::exception& ex) {
                    toast(std::string("无法打开：") + ex.what(), 4);
                }
                break;
            }
            if (remove) {
                std::string name = e->attachments[(size_t)selected].name;
                repo_.remove_attachment(id, selected);
                sync_mtime();
                toast("已移除附件：" + name + "（Ctrl+Z 可撤销）", 4);
                continue;
            }
            if (key.type == Key::Type::Char && key.ch == 'q') break;
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

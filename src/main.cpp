// 墨记 MoJi · C++17 版命令行入口
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#include <shellapi.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "core/importer.hpp"
#include "core/repository.hpp"
#include "core/util.hpp"
#include "demo.hpp"
#include "gui/gui.hpp"
#include "tui/tui.hpp"

using namespace moji;

namespace {

const char* kVersion = "1.0.0";
bool g_color = false;

fs::path u8path(const std::string& s) {
#ifdef _WIN32
    return fs::path(to_wide(s));
#else
    return fs::path(s);
#endif
}

std::string paint(const std::string& text, const char* code) {
    if (!g_color) {
        return text;
    }
    return std::string(code) + text + "\x1b[0m";
}

void print_events(const std::vector<Event*>& list, const std::string& title) {
    if (!title.empty()) {
        std::cout << "\n" << paint(title, "\x1b[1;35m") << "\n";
    }
    if (list.empty()) {
        std::cout << paint("  （暂无记录）", "\x1b[90m") << "\n";
        return;
    }
    std::cout << paint(pad_display("优先级", 8) + pad_display("状态", 6) +
                           pad_display("标题", 32) + pad_display("截止", 22) + "标签",
                       "\x1b[90m")
              << "\n";
    std::cout << paint(std::string(80, '-'), "\x1b[90m") << "\n";
    for (const Event* e : list) {
        std::string mark = e->done ? "✓" : "○";
        std::string status = paint(mark, e->done ? "\x1b[32m" : "\x1b[90m");
        std::string due = e->due_text();
        const char* due_color = "\x1b[36m";
        if (e->done) {
            due_color = "\x1b[90m";
        } else if (e->is_overdue()) {
            due_color = "\x1b[31m";
        } else if (e->is_due_today()) {
            due_color = "\x1b[33m";
        }
        std::string title_text = truncate_display(e->title, 28);
        if (e->done) {
            title_text = paint(title_text, "\x1b[90m");
        }
        std::string tags;
        for (const auto& t : e->tags) {
            tags += paint("#" + t + " ", "\x1b[34m");
        }
        std::cout << pad_display("P" + std::to_string(e->priority), 8)
                  << pad_display(status, 6) << pad_display(title_text, 32)
                  << paint(pad_display(due, 22), due_color) << tags << "\n";
    }
    std::cout << paint("  共 " + std::to_string(list.size()) + " 条", "\x1b[90m") << "\n";
}

std::string resolve_id(Repository& repo, const std::string& token_in) {
    std::string token = trim(token_in);
    while (!token.empty() && token.front() == '#') {
        token.erase(token.begin());
    }
    if (token.empty()) {
        throw RepoError("请给出事件编号或 id");
    }
    if (std::all_of(token.begin(), token.end(), ::isdigit)) {
        int idx = std::atoi(token.c_str());
        Event* e = repo.events.at(idx);
        if (!e) {
            throw RepoError("没有第 " + std::to_string(idx) + " 条记录");
        }
        return e->id;
    }
    std::vector<std::string> matches;
    for (Event& e : repo.events) {
        if (e.id == token) {
            return e.id;
        }
        if (e.id.rfind(token, 0) == 0) {
            matches.push_back(e.id);
        }
    }
    if (matches.size() == 1) {
        return matches[0];
    }
    if (matches.empty()) {
        throw RepoError("找不到编号为 " + token + " 的记录");
    }
    throw RepoError("编号前缀不唯一");
}

void snapshot_before_write(Repository& repo) {
    try {
        repo.store.snapshot("undo");
        repo.store.clear_snapshots("redo");
    } catch (...) {
    }
}

void print_help() {
    std::cout << "墨记 MoJi " << kVersion << " · C++17 电子记事本\n\n"
              << "用法：\n"
              << "  moji [--data-dir DIR]                启动 TUI（默认）\n"
              << "  moji gui [--screenshot FILE]         启动 MD 风格 GUI\n"
              << "  moji add 标题 [-u 时间] [-t 标签] [-p 位置] [-r 提前分钟] [-n 备注]\n"
              << "  moji list [--today|--date YYYY-MM-DD|--overdue|--pending|--done]\n"
              << "              [--tag 标签] [--search 关键字] [--limit N]\n"
              << "  moji show 3 | moji done 3 [--undo] | moji rm 3\n"
              << "  moji search 关键字 | moji today | moji stats\n"
              << "  moji undo [--redo]\n"
              << "  moji export 文件(.mjnote/.json/.txt/.md)\n"
              << "  moji import 文件 [--replace]\n"
              << "  moji seed-demo [--force]\n"
              << "  moji doctor\n\n"
              << "数据目录默认 ~/.moji，可用 --data-dir 或环境变量 MOJI_HOME 指定。\n";
}

int cmd_add(Repository& repo, int argc, char** argv, int start) {
    std::string title, note, due_text;
    std::vector<std::string> tags;
    int position = 0;
    int remind = 0;
    for (int i = start; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-u" || a == "--due") {
            if (++i < argc) due_text = argv[i];
        } else if (a == "-t" || a == "--tag") {
            if (++i < argc) {
                auto parts = split_tags(argv[i]);
                tags.insert(tags.end(), parts.begin(), parts.end());
            }
        } else if (a == "-p" || a == "--pos") {
            if (++i < argc) position = std::atoi(argv[i]);
        } else if (a == "-r" || a == "--remind") {
            if (++i < argc) remind = std::atoi(argv[i]);
        } else if (a == "-n" || a == "--note") {
            if (++i < argc) note = argv[i];
        } else if (!a.empty() && a[0] == '-') {
            throw RepoError("未知参数：" + a);
        } else {
            if (!title.empty()) title += " ";
            title += a;
        }
    }
    if (title.empty()) {
        throw RepoError("请填写标题");
    }
    std::optional<std::time_t> due;
    if (!due_text.empty()) {
        due = parse_datetime(due_text);
        if (!due.has_value()) {
            throw RepoError("看不懂这个时间：" + due_text);
        }
    }
    snapshot_before_write(repo);
    Event& e = repo.add(title, note, due, tags,
                        position > 0 ? std::optional<int>(position) : std::nullopt,
                        remind);
    std::cout << paint("已添加 P" + std::to_string(e.priority) + "：" + e.title +
                           "  [" + humanize_due(e.due) + "]",
                       "\x1b[32m")
              << "\n";
    return 0;
}

int cmd_list(Repository& repo, int argc, char** argv, int start) {
    bool today = false, overdue = false, pending = false, done = false;
    std::string date_text, tag, search, limit_text;
    for (int i = start; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--today") today = true;
        else if (a == "--overdue") overdue = true;
        else if (a == "--pending") pending = true;
        else if (a == "--done") done = true;
        else if (a == "--date") { if (++i < argc) date_text = argv[i]; }
        else if (a == "--tag") { if (++i < argc) tag = argv[i]; }
        else if (a == "--search" || a == "-s") { if (++i < argc) search = argv[i]; }
        else if (a == "--limit") { if (++i < argc) limit_text = argv[i]; }
        else throw RepoError("未知参数：" + a);
    }
    std::vector<Event*> list;
    std::string title;
    if (!date_text.empty()) {
        auto day = parse_date(date_text);
        if (!day.has_value()) throw RepoError("无法识别的日期：" + date_text);
        list = repo.due_on(*day);
        title = date_text + " 的安排";
    } else if (today) {
        list = repo.due_on(now_time());
        title = "今日安排";
    } else if (overdue) {
        list = repo.overdue();
        title = "已逾期";
    } else if (pending) {
        list = repo.pending();
        title = "未完成";
    } else if (done) {
        list = repo.finished();
        title = "已完成";
    } else if (!tag.empty()) {
        for (Event* e : repo.all()) {
            if (std::find(e->tags.begin(), e->tags.end(), tag) != e->tags.end()) {
                list.push_back(e);
            }
        }
        title = "标签 #" + tag;
    } else if (!search.empty()) {
        list = repo.search(search);
        title = "搜索「" + search + "」";
    } else {
        list = repo.all();
        title = "全部安排";
    }
    if (!limit_text.empty()) {
        int limit = std::atoi(limit_text.c_str());
        if (limit > 0 && (int)list.size() > limit) {
            list.resize((size_t)limit);
        }
    }
    print_events(list, title);
    return 0;
}

int cmd_show(Repository& repo, const std::string& id_token) {
    std::string id = resolve_id(repo, id_token);
    Event* e = repo.find(id);
    if (!e) throw RepoError("记录不存在");
    std::cout << paint("P" + std::to_string(e->priority) + "  " + e->title,
                       "\x1b[1;35m")
              << "\n";
    std::cout << "状态：" << (e->done ? "已完成" : "进行中") << "\n";
    std::cout << "截止：" << humanize_due(e->due)
              << (e->due.has_value() ? "  (" + iso_from_time(*e->due) + ")" : "") << "\n";
    std::cout << "标签：" << (e->tags.empty() ? "—" : join(e->tags, " ")) << "\n";
    std::cout << "创建：" << (e->created_at.has_value() ? iso_from_time(*e->created_at) : "")
              << "　更新："
              << (e->updated_at.has_value() ? iso_from_time(*e->updated_at) : "") << "\n";
    std::cout << "提醒：提前 " << e->remind_before << " 分钟\n";
    if (!e->attachments.empty()) {
        std::cout << "附件：\n";
        for (size_t i = 0; i < e->attachments.size(); ++i) {
            const Attachment& a = e->attachments[i];
            std::cout << "  " << (i + 1) << ". " << a.name << "  [" << a.mime << "] "
                      << human_size(a.size) << " "
                      << (repo.store.attachment_exists(a) ? "✓" : "✗ 丢失") << "\n";
        }
    }
    if (!e->note.empty()) {
        std::cout << "\n" << e->note << "\n";
    }
    return 0;
}

int cmd_stats(Repository& repo) {
    Stats s = repo.stats();
    std::cout << paint("墨记 · 数据概览", "\x1b[1;35m") << "\n";
    std::cout << "  全部 " << s.total << " 条　待办 " << s.pending << "　已完成 "
              << s.done << "　完成率 " << s.rate << "%\n";
    std::cout << "  今天 " << s.today << "　逾期 " << s.overdue << "　附件 "
              << s.attachments << "\n";
    std::cout << "  数据目录：" << repo.display_dir() << "\n";
    auto tags = repo.tags();
    if (!tags.empty()) {
        std::cout << "  标签：";
        for (const auto& kv : tags) {
            std::cout << "#" << kv.first << "×" << kv.second << "  ";
        }
        std::cout << "\n";
    }
    return 0;
}

int cmd_seed(Repository& repo, bool force) {
    if (!repo.events.empty() && !force) {
        throw RepoError("当前数据目录已有记录；如仍要写入演示数据，请加 --force");
    }
    int n = seed_demo(repo);
    std::cout << paint("已写入 " + std::to_string(n) + " 条演示数据到 " + repo.display_dir(),
                       "\x1b[32m")
              << "\n";
    return 0;
}

int cmd_export(Repository& repo, const std::string& path) {
    fs::path out = export_auto(repo, u8path(path));
    std::cout << paint("已导出到 " + path, "\x1b[32m") << "\n";
    (void)out;
    return 0;
}

int cmd_import(Repository& repo, const std::string& path, bool replace) {
    snapshot_before_write(repo);
    ImportReport report = import_auto(repo, u8path(path), !replace);
    std::cout << paint("导入完成：新增 " + std::to_string(report.added) + " 条，跳过 " +
                           std::to_string(report.skipped) + " 条",
                       "\x1b[32m")
              << "\n";
    return 0;
}

int cmd_undo(Repository& repo, bool redo) {
    bool ok = false;
    if (redo) {
        if (repo.can_redo()) {
            ok = repo.redo();
        } else if (repo.store.has_snapshots("redo")) {
            repo.store.snapshot("undo");
            ok = repo.store.restore_last("redo");
            if (ok) repo.reload();
        }
    } else {
        if (repo.can_undo()) {
            ok = repo.undo();
        } else if (repo.store.has_snapshots("undo")) {
            repo.store.snapshot("redo");
            ok = repo.store.restore_last("undo");
            if (ok) repo.reload();
        }
    }
    std::cout << paint(redo ? (ok ? "已重做" : "没有可重做的操作")
                            : (ok ? "已撤销" : "没有可撤销的操作"),
                       ok ? "\x1b[32m" : "\x1b[90m")
              << "\n";
    return ok ? 0 : 1;
}

int cmd_doctor(Repository& repo) {
    std::cout << paint(std::string("墨记 MoJi ") + kVersion + " (C++17)", "\x1b[1;35m") << "\n";
    std::cout << "  数据目录：" << repo.display_dir() << "\n";
    std::error_code ec;
    auto size = fs::file_size(repo.store.events_path, ec);
    std::cout << "  events.json：" << (ec ? 0 : (long long)size) << " 字节\n";
    Stats s = repo.stats();
    std::cout << "  事件：共 " << s.total << " 条，待办 " << s.pending << "，已完成 "
              << s.done << "，今天 " << s.today << "，逾期 " << s.overdue << "\n";
    std::cout << "  附件：" << s.attachments << " 个\n";
    bool warn = !repo.warnings.empty();
    for (const auto& w : repo.warnings) {
        std::cout << paint("  警告：" + w, "\x1b[33m") << "\n";
    }
    std::cout << paint(warn ? "  数据检查：有警告" : "  数据检查：通过",
                       warn ? "\x1b[33m" : "\x1b[32m")
              << "\n";
    fs::path probe = repo.store.dir / ".write-test";
    bool ok = write_file_atomic(probe, "ok");
    fs::remove(probe, ec);
    std::cout << paint(ok ? "  写权限：正常" : "  写权限：异常",
                       ok ? "\x1b[32m" : "\x1b[31m")
              << "\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    // Windows 的 argv 是系统代码页（GBK）；统一转成 UTF-8
    {
        int wargc = 0;
        LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
        if (wargv) {
            std::vector<std::string> storage;
            storage.reserve((size_t)wargc);
            for (int k = 0; k < wargc; ++k) {
                storage.push_back(to_utf8(wargv[k]));
            }
            LocalFree(wargv);
            static std::vector<std::string> keep;
            keep = std::move(storage);
            static std::vector<char*> ptrs;
            ptrs.clear();
            for (auto& item : keep) {
                ptrs.push_back(item.data());
            }
            ptrs.push_back(nullptr);
            argc = (int)keep.size();
            argv = ptrs.data();
        }
    }
    SetConsoleOutputCP(65001);
    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (hout && GetConsoleMode(hout, &mode)) {
        SetConsoleMode(hout, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    g_color = isatty(fileno(stdout)) != 0;
#else
    g_color = isatty(fileno(stdout)) != 0;
#endif

    std::string data_dir;
    int i = 1;
    while (i < argc) {
        std::string a = argv[i];
        if (a == "-d" || a == "--data-dir") {
            if (++i < argc) data_dir = argv[i];
            ++i;
        } else if (a == "-V" || a == "--version") {
            std::cout << "墨记 MoJi " << kVersion << " (C++17)\n";
            return 0;
        } else if (a == "-h" || a == "--help") {
            print_help();
            return 0;
        } else {
            break;
        }
    }

    try {
        Repository repo(data_dir.empty() ? fs::path{} : u8path(data_dir));
        for (const auto& w : repo.warnings) {
            std::cerr << paint("提示：" + w, "\x1b[33m") << "\n";
        }

        std::string command = i < argc ? argv[i++] : "";
        if (command.empty()) {
            if (isatty(fileno(stdin)) && isatty(fileno(stdout))) {
                return run_tui(repo);
            }
            print_help();
            return 0;
        }
        if (command == "tui") {
            return run_tui(repo);
        }
        if (command == "gui") {
            std::string shot = std::getenv("MOJI_SHOT") ? std::getenv("MOJI_SHOT") : "";
            for (int k = i; k < argc; ++k) {
                std::string a = argv[k];
                if (a == "--screenshot" && k + 1 < argc) {
                    shot = argv[++k];
                }
            }
            return run_gui(repo, shot);
        }
        if (command == "add") return cmd_add(repo, argc, argv, i);
        if (command == "list") return cmd_list(repo, argc, argv, i);
        if (command == "show") {
            if (i >= argc) throw RepoError("请给出编号");
            return cmd_show(repo, argv[i]);
        }
        if (command == "done" || command == "rm" || command == "remove") {
            if (i >= argc) throw RepoError("请给出编号");
            bool undo = false;
            for (int k = i + 1; k < argc; ++k) {
                if (std::string(argv[k]) == "--undo") undo = true;
            }
            snapshot_before_write(repo);
            std::string id = resolve_id(repo, argv[i]);
            if (command == "done") {
                Event& e = repo.set_done(id, !undo);
                std::cout << paint((undo ? "已撤销完成：" : "已完成：") + e.title, "\x1b[32m")
                          << "\n";
            } else {
                Event* e = repo.find(id);
                std::string title = e ? e->title : id;
                if (!repo.remove(id)) throw RepoError("删除失败");
                std::cout << paint("已删除：" + title, "\x1b[33m") << "\n";
            }
            return 0;
        }
        if (command == "search") {
            std::string keyword;
            for (int k = i; k < argc; ++k) {
                if (!keyword.empty()) keyword += " ";
                keyword += argv[k];
            }
            if (keyword.empty()) throw RepoError("请给出关键字");
            print_events(repo.search(keyword), "搜索「" + keyword + "」");
            return 0;
        }
        if (command == "today") {
            print_events(repo.due_on(now_time()), "今日安排");
            return 0;
        }
        if (command == "stats") return cmd_stats(repo);
        if (command == "undo") {
            bool redo = false;
            for (int k = i; k < argc; ++k) {
                if (std::string(argv[k]) == "--redo") redo = true;
            }
            return cmd_undo(repo, redo);
        }
        if (command == "export") {
            if (i >= argc) throw RepoError("请给出导出路径");
            return cmd_export(repo, argv[i]);
        }
        if (command == "import") {
            if (i >= argc) throw RepoError("请给出来源文件");
            bool replace = false;
            for (int k = i + 1; k < argc; ++k) {
                if (std::string(argv[k]) == "--replace") replace = true;
            }
            return cmd_import(repo, argv[i], replace);
        }
        if (command == "seed-demo") {
            bool force = false;
            for (int k = i; k < argc; ++k) {
                if (std::string(argv[k]) == "--force") force = true;
            }
            snapshot_before_write(repo);
            return cmd_seed(repo, force);
        }
        if (command == "doctor") return cmd_doctor(repo);
        throw RepoError("未知命令：" + command);
    } catch (const std::exception& e) {
        std::cerr << paint(std::string("操作失败：") + e.what(), "\x1b[31m") << "\n";
        return 1;
    }
}

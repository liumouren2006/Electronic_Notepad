// C++ 核心回归测试：g++ 编译后直接运行，失败返回非零
#include <cstdio>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "core/elist.hpp"
#include "core/importer.hpp"
#include "core/model.hpp"
#include "core/repository.hpp"
#include "core/store.hpp"
#include "core/util.hpp"
#include "core/zip.hpp"

using namespace moji;

namespace {
int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  "      \
                      << #cond << "\n";                                      \
        }                                                                    \
    } while (0)

#define CHECK_EQ(a, b)                                                       \
    do {                                                                     \
        ++g_checks;                                                          \
        auto _a = (a);                                                       \
        auto _b = (b);                                                       \
        if (!(_a == _b)) {                                                   \
            ++g_failures;                                                    \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  "      \
                      << #a << " != " << #b << "\n";                          \
        }                                                                    \
    } while (0)

fs::path temp_dir(const char* tag) {
    fs::path base = fs::temp_directory_path() / (std::string("moji-cpp-") + tag +
                                                  "-" + random_hex(6));
    fs::create_directories(base);
    return base;
}

void test_list() {
    EventList list;
    Event a = Event::create("买菜");
    Event b = Event::create("写作业");
    list.append(a);
    list.append(b);
    CHECK_EQ(list.at(1)->priority, 1);
    CHECK_EQ(list.at(2)->priority, 2);
    Event c = Event::create("取快递");
    list.insert_at(2, c);
    CHECK_EQ(list.at(1)->title, "买菜");
    CHECK_EQ(list.at(2)->title, "取快递");
    CHECK_EQ(list.at(3)->title, "写作业");
    CHECK_EQ(list.at(3)->priority, 3);
    CHECK_EQ(list.size(), (size_t)3);
    list.move_to(list.at(3)->id, 1);
    CHECK_EQ(list.at(1)->title, "写作业");
    CHECK_EQ(list.at(2)->priority, 2);
    auto removed = list.remove(list.at(2)->id);
    CHECK(removed.has_value());
    CHECK_EQ(list.size(), (size_t)2);
    CHECK_EQ(list.at(1)->priority, 1);
    CHECK_EQ(list.at(2)->priority, 2);
}

void test_repository() {
    fs::path dir = temp_dir("repo");
    {
        Repository repo(dir);
        Event& a = repo.add("第一件事", "备注", std::nullopt, {"课程"});
        Event& b = repo.add("第二件事");
        CHECK_EQ(a.priority, 1);
        CHECK_EQ(b.priority, 2);
        Event& c = repo.add("插队", "", std::nullopt, {}, 2);
        const std::string aid = a.id;
        const std::string bid = b.id;
        const std::string cid = c.id;
        CHECK_EQ(c.priority, 2);
        CHECK_EQ(repo.find(bid)->priority, 3);
        CHECK_EQ(repo.events.at(1)->title, "第一件事");
        CHECK_EQ(repo.events.at(2)->title, "插队");
        repo.update(aid, "第一件事（改）", "新备注", parse_datetime("2026-12-01 10:00"),
                    {"课程", "重要"}, 15, std::nullopt, std::nullopt, std::nullopt, true);
        CHECK_EQ(repo.find(aid)->title, "第一件事（改）");
        CHECK_EQ(repo.find(aid)->tags.size(), (size_t)2);
        CHECK(repo.find(aid)->due.has_value());
        CHECK(repo.find(aid)->system_notify);
        repo.set_visual(aid, "#ff0055", "â", "{\"text\":{\"x\":1}}");
        CHECK_EQ(repo.find(aid)->color, "#ff0055");
        CHECK_EQ(repo.find(aid)->icon, "â");
        CHECK(!repo.find(aid)->page_layout.empty());
        repo.set_done(bid, true);
        CHECK(repo.find(bid)->done);
        repo.move_by(cid, -1);
        CHECK_EQ(repo.find(cid)->priority, 1);
        CHECK(repo.undo());
        CHECK_EQ(repo.find(cid)->priority, 2);
        CHECK(repo.redo());
        CHECK_EQ(repo.find(cid)->priority, 1);
        // 持久化
        Repository again(dir);
        CHECK_EQ(again.events.size(), (size_t)3);
        CHECK_EQ(again.events.at(1)->title, "插队");
        CHECK_EQ(again.find(aid)->icon, "â");
        CHECK_EQ(again.find(aid)->color, "#ff0055");
        CHECK(!again.find(aid)->page_layout.empty());
        CHECK(again.find(aid)->system_notify);
        CHECK_EQ(again.find(aid)->ringtone, std::string());

        // 每事项铃声 + 稍后提醒
        repo.set_ringtone(aid, "C:/ring.mp3");
        CHECK_EQ(repo.find(aid)->ringtone, "C:/ring.mp3");
        // 构造一个“已经到点”的事项：截止 10:00，提前 0 分钟
        Event& timed = repo.add("闹钟测试", "", parse_datetime("2026-12-02 10:00"), {}, 0);
        const std::string tid = timed.id;
        auto has_id = [](const std::vector<Event*>& list, const std::string& id) {
            for (Event* e : list) {
                if (e->id == id) return true;
            }
            return false;
        };
        CHECK(!has_id(repo.due_reminders(parse_datetime("2026-12-02 09:59:59").value()), tid));
        // 10:00:00 整点就应提醒
        auto at_1000 = parse_datetime("2026-12-02 10:00:00").value();
        CHECK(has_id(repo.due_reminders(at_1000), tid));
        repo.mark_reminded(repo.due_reminders(at_1000), at_1000);
        CHECK(!has_id(repo.due_reminders(at_1000), tid));
        // 稍后 5 分钟：在 10:05 之前不响，到点后再次提醒
        auto snooze_at = at_1000 + 300;
        repo.set_snooze(tid, snooze_at);
        CHECK(!has_id(repo.due_reminders(at_1000 + 60), tid));
        CHECK(has_id(repo.due_reminders(at_1000 + 300), tid));
        auto next = repo.next_reminder_at(at_1000 + 60);
        CHECK(next.has_value());
        CHECK_EQ((long long)*next, (long long)snooze_at);
        repo.set_snooze(tid, std::nullopt);
        repo.remove(tid);
        // 查询
        CHECK_EQ(again.pending().size(), (size_t)2);
        CHECK_EQ(again.finished().size(), (size_t)1);
        CHECK_EQ(again.search("备注").size(), (size_t)1);
        CHECK(again.tags().count("课程") == 1);
        // 附件
        fs::path file = dir / "a.txt";
        write_file_atomic(file, "hello");
        Attachment& att = again.add_attachment(again.events.at(1)->id, file);
        CHECK_EQ(att.mime, "text/plain");
        CHECK(again.store.attachment_exists(att));
        // 路径穿越
        bool blocked = false;
        try {
            again.store.resolve("../../windows/win.ini");
        } catch (...) {
            blocked = true;
        }
        CHECK(blocked);
    }
    std::error_code ec;
    fs::remove_all(dir, ec);
}

void test_store_recovery() {
    fs::path dir = temp_dir("recover");
    {
        Repository repo(dir);
        repo.add("重要数据");
        CHECK(fs::exists(repo.store.events_path));
        write_file_atomic(repo.store.events_path, "{ not json");
        Repository repaired(dir);
        CHECK_EQ(repaired.events.size(), (size_t)1);
        CHECK(!repaired.warnings.empty());
    }
    std::error_code ec;
    fs::remove_all(dir, ec);
}

void test_zip() {
    std::vector<std::pair<std::string, Bytes>> entries;
    entries.push_back({"a.txt", Bytes{'h', 'i'}});
    entries.push_back({"dir/b.bin", Bytes{0, 1, 2, 255}});
    Bytes zipped = zip_write(entries);
    auto read = zip_read(zipped);
    CHECK_EQ(read.size(), (size_t)2);
    CHECK(std::string(read["a.txt"].begin(), read["a.txt"].end()) == "hi");
    CHECK_EQ(read["dir/b.bin"].size(), (size_t)4);
}

void test_import_export() {
    fs::path dir = temp_dir("io");
    fs::path out = dir / "out";
    fs::create_directories(out);
    {
        Repository repo(dir / "a");
        Event& e = repo.add("导出任务", "备注", parse_datetime("2026-12-02 10:00"),
                            {"课程"}, std::nullopt, 30);
        fs::path img = dir / "cover.bmp";
        Bytes bmp = {'B', 'M', 0, 0, 0, 0};
        write_bytes_atomic(img, bmp);
        repo.add_attachment(e.id, img);

        fs::path bundle = out / "pack.mjnote";
        export_bundle(repo, bundle);
        CHECK(fs::exists(bundle));

        Repository other(dir / "b");
        ImportReport report = import_bundle(other, bundle, true);
        CHECK_EQ(report.added, 1);
        CHECK_EQ(other.events.size(), (size_t)1);
        Event* imported = other.events.at(1);
        CHECK_EQ(imported->title, "导出任务");
        CHECK_EQ(imported->attachments.size(), (size_t)1);
        CHECK(other.store.attachment_exists(imported->attachments[0]));

        fs::path todo = out / "todo.txt";
        export_todotxt(repo, todo);
        Repository todo_repo(dir / "c");
        ImportReport tr = import_todotxt(todo_repo, todo, true);
        CHECK_EQ(tr.added, 1);
        CHECK_EQ(todo_repo.events.at(1)->title, "导出任务");
        CHECK(todo_repo.events.at(1)->due.has_value());

        fs::path md = out / "out.md";
        export_markdown(repo, md);
        std::string text = read_file(md);
        CHECK(text.find("墨记") != std::string::npos);
        CHECK(text.find("导出任务") != std::string::npos);

        fs::path json = out / "out.json";
        export_json(repo, json);
        Repository json_repo(dir / "d");
        import_json(json_repo, json, true);
        CHECK_EQ(json_repo.events.size(), (size_t)1);
    }
    std::error_code ec;
    fs::remove_all(dir, ec);
}

void test_reminder() {
    std::time_t now = now_time();
    std::optional<std::time_t> due = now + 300;
    Event e = Event::create("提醒测试", "", due, {}, 10);
    CHECK(e.needs_reminder(now));
    e.last_reminded_at = now;
    CHECK(!e.needs_reminder(now));
    Event done = Event::create("已完成", "", now, {}, 0);
    done.done = true;
    CHECK(!done.needs_reminder(now));
    CHECK(humanize_due(due, now).find("分钟后") != std::string::npos);
}

void test_sync_merge() {
    fs::path dir = temp_dir("sync");
    {
        Repository repo(dir);
        Event& a = repo.add("本地较新");
        a.updated_at = 100;
        std::string a_id = a.id;
        auto a_created = a.created_at;
        Event& b = repo.add("本地较旧");
        b.updated_at = 300;
        std::string b_id = b.id;
        repo.save();

        Event remote_older = *repo.find(a_id);
        remote_older.title = "远端旧版本";
        remote_older.updated_at = 50;

        Event remote_newer = *repo.find(b_id);
        remote_newer.title = "远端新版本";
        remote_newer.updated_at = 400;

        Event remote_new = Event::create("云端新增");
        remote_new.id = "cloudnew1";
        remote_new.created_at = 500;
        remote_new.updated_at = 500;
        remote_new.priority = 9;

        Event duplicate = Event::create("本地较新");
        duplicate.id = "dupid999";
        duplicate.created_at = a_created;
        duplicate.updated_at = 999;

        SyncReport report = repo.sync_merge({remote_older, remote_newer, remote_new, duplicate});
        CHECK_EQ(report.added, 1);
        CHECK_EQ(report.pulled, 2);
        CHECK_EQ(report.kept_local, 1);
        CHECK_EQ(report.skipped, 1);
        CHECK(repo.find(a_id) != nullptr);
        CHECK_EQ(repo.find(a_id)->title, std::string("本地较新"));
        CHECK_EQ(repo.find(b_id)->title, std::string("远端新版本"));
        CHECK(repo.find("cloudnew1") != nullptr);
        CHECK(repo.find("dupid999") == nullptr);
        CHECK_EQ(repo.events.size(), (size_t)3);
        int expected = 1;
        for (const Event& e : repo.events) {
            CHECK_EQ(e.priority, expected++);
        }
        CHECK_EQ(repo.find("cloudnew1")->priority, 3);
    }
    std::error_code ec;
    fs::remove_all(dir, ec);
}

void test_tombstones() {
    fs::path dir = temp_dir("tomb");
    {
        Repository repo(dir);
        Event& a = repo.add("保留");
        Event& b = repo.add("待删除");
        std::string a_id = a.id, b_id = b.id;
        a.updated_at = 200;
        b.updated_at = 100;
        repo.save();
        std::map<std::string, std::time_t> tombstones{{b_id, 150}};
        CHECK_EQ(repo.apply_tombstones(tombstones), 1);
        CHECK(repo.find(a_id) != nullptr);
        CHECK(repo.find(b_id) == nullptr);
        Event& c = repo.add("已更新");
        std::string c_id = c.id;
        c.updated_at = 300;
        repo.save();
        std::map<std::string, std::time_t> older{{c_id, 200}};
        CHECK_EQ(repo.apply_tombstones(older), 0);
        CHECK(repo.find(c_id) != nullptr);
    }
    std::error_code ec;
    fs::remove_all(dir, ec);
}

void test_cli_style() {
    fs::path dir = temp_dir("cli");
    {
        Repository repo(dir);
        repo.store.snapshot("undo");
        repo.add("可撤销");
        repo.store.snapshot("undo");
        repo.add("第二条");
        CHECK(repo.store.has_snapshots("undo"));
        CHECK(repo.store.restore_last("undo"));
        Repository reloaded(dir);
        CHECK_EQ(reloaded.events.size(), (size_t)1);
    }
    std::error_code ec;
    fs::remove_all(dir, ec);
}

}  // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    test_list();
    test_repository();
    test_store_recovery();
    test_zip();
    test_import_export();
    test_reminder();
    test_sync_merge();
    test_tombstones();
    test_cli_style();
    std::cout << "checks=" << g_checks << " failures=" << g_failures << "\n";
    if (g_failures == 0) {
        std::cout << "ALL C++ CORE TESTS PASSED\n";
        return 0;
    }
    return 1;
}

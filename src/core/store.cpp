#include "store.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "util.hpp"

namespace moji {

namespace {
std::string default_data_dir_utf8() {
    if (const char* env = std::getenv("MOJI_HOME"); env && *env) {
#ifdef _WIN32
        return env;
#else
        return env;
#endif
    }
    fs::path home = home_dir();
#ifdef _WIN32
    return to_utf8((home / L".moji").wstring());
#else
    return (home / ".moji").string();
#endif
}

fs::path make_path(const std::string& utf8) {
#ifdef _WIN32
    return fs::path(to_wide(utf8));
#else
    return fs::path(utf8);
#endif
}

bool is_safe_segment(const std::string& s) {
    return !s.empty() && s != "." && s != ".." && s.find('/') == std::string::npos &&
           s.find('\\') == std::string::npos && s.find(':') == std::string::npos;
}
}  // namespace

Store::Store(const fs::path& data_dir) {
    if (data_dir.empty()) {
        dir = make_path(default_data_dir_utf8());
    } else {
        dir = data_dir;
    }
    std::error_code ec;
    dir = fs::absolute(dir, ec);
    if (ec) {
        dir = fs::path(data_dir);
    }
    events_path = dir / "events.json";
    backup_path = dir / "events.json.bak";
    config_path = dir / "config.json";
    attachments_dir = dir / "attachments";
    backups_dir = dir / "backups";
    ensure_layout();
}

void Store::ensure_layout() const {
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        throw StoreError("无法创建数据目录：" + display_dir());
    }
    fs::create_directories(attachments_dir, ec);
    fs::create_directories(backups_dir, ec);
}

std::string Store::timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  now.time_since_epoch())
                  .count() %
              1000000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm);
    char out[80];
    std::snprintf(out, sizeof(out), "%s-%06lld", buf, (long long)us);
    return out;
}

std::vector<json> Store::load_events() {
    warnings.clear();
    auto try_load = [&](const fs::path& path, bool is_backup) -> std::optional<std::vector<json>> {
        bool ok = false;
        std::string text = read_file(path, &ok);
        if (!ok || trim(text).empty()) {
            return std::nullopt;
        }
        try {
            json data = json::parse(text);
            json arr = json::array();
            if (data.is_object() && data.contains("events") && data["events"].is_array()) {
                arr = data["events"];
            } else if (data.is_array()) {
                arr = data;
            } else {
                throw std::runtime_error("顶层不是事件数组");
            }
            std::vector<json> out;
            out.reserve(arr.size());
            for (const auto& item : arr) {
                if (item.is_object()) {
                    out.push_back(item);
                }
            }
            if (is_backup) {
                warnings.push_back("主数据损坏，已从备份恢复：" +
#ifdef _WIN32
                                   to_utf8(path.filename().wstring())
#else
                                   path.filename().string()
#endif
                );
                write_file_atomic(events_path, text);
            }
            return out;
        } catch (const std::exception& e) {
            warnings.push_back("读取 " +
#ifdef _WIN32
                               to_utf8(path.filename().wstring())
#else
                               path.filename().string()
#endif
                               + " 失败：" + e.what());
            if (!is_backup) {
                std::error_code ec;
                fs::rename(path, dir / ("events.json.corrupt-" + timestamp()), ec);
            }
            return std::nullopt;
        }
    };

    if (auto events = try_load(events_path, false)) {
        return *events;
    }
    if (auto events = try_load(backup_path, true)) {
        return *events;
    }
    return {};
}

void Store::save_events(const std::vector<json>& events) {
    json payload;
    payload["app"] = "moji-note";
    payload["version"] = 1;
    payload["updated_at"] = now_iso();
    payload["count"] = events.size();
    payload["events"] = json::array();
    for (const auto& e : events) {
        payload["events"].push_back(e);
    }
    std::string text = payload.dump(2, ' ', false);
    if (!write_file_atomic(events_path, text)) {
        throw StoreError("保存失败：无法写入 " + display_dir());
    }
    write_file_atomic(backup_path, text);
    write_backups(text);
}

void Store::write_backups(const std::string& text) const {
    std::error_code ec;
    fs::path target = backups_dir / ("events-" + timestamp() + ".json");
    write_file_atomic(target, text);
    int keep = 20;
    try {
        json cfg = load_config();
        keep = cfg.value("keep_backups", 20);
    } catch (...) {
    }
    auto files = backup_files("events-");
    if (keep <= 0) {
        keep = 0;
    }
    int excess = (int)files.size() - keep;
    for (int i = 0; i < excess && i < (int)files.size(); ++i) {
        fs::remove(files[(size_t)i], ec);
    }
}

std::vector<fs::path> Store::backup_files(const std::string& prefix) const {
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(backups_dir, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string name =
#ifdef _WIN32
            to_utf8(entry.path().filename().wstring());
#else
            entry.path().filename().string();
#endif
        if (name.rfind(prefix, 0) == 0 && name.size() > 5 &&
            name.substr(name.size() - 5) == ".json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

json Store::load_config() const {
    json cfg;
    cfg["version"] = 1;
    cfg["theme"] = "light";
    cfg["beep"] = true;
    cfg["remind_interval_sec"] = 30;
    cfg["keep_backups"] = 20;
    cfg["window"] = json{{"w", 1280}, {"h", 820}};
    bool ok = false;
    std::string text = read_file(config_path, &ok);
    if (ok && !trim(text).empty()) {
        try {
            json data = json::parse(text);
            if (data.is_object()) {
                for (auto it = data.begin(); it != data.end(); ++it) {
                    cfg[it.key()] = it.value();
                }
            }
        } catch (...) {
        }
    }
    return cfg;
}

void Store::save_config(const json& config) const {
    json merged = load_config();
    for (auto it = config.begin(); it != config.end(); ++it) {
        merged[it.key()] = it.value();
    }
    write_file_atomic(config_path, merged.dump(2, ' ', false));
}

std::string Store::sanitize_name(const std::string& raw) const {
    std::string name = raw;
    auto pos = name.find_last_of("/\\");
    if (pos != std::string::npos) {
        name = name.substr(pos + 1);
    }
    std::string out;
    for (unsigned char c : name) {
        if (c < 32 || std::string("<>:\"/\\|?*").find((char)c) != std::string::npos) {
            out.push_back('_');
        } else {
            out.push_back((char)c);
        }
    }
    out = trim(out);
    while (!out.empty() && out.back() == '.') {
        out.pop_back();
    }
    if (out.empty()) {
        out = "file";
    }
    if (out.size() > 120) {
        out = out.substr(0, 120);
    }
    return out;
}

Attachment Store::add_attachment(const std::string& event_id, const fs::path& src) {
    std::error_code ec;
    if (!fs::is_regular_file(src, ec)) {
        throw StoreError("附件不存在或不是文件");
    }
#ifdef _WIN32
    std::string base = to_utf8(src.filename().wstring());
#else
    std::string base = src.filename().string();
#endif
    std::string safe_id = is_safe_segment(event_id) ? event_id : "misc";
    fs::path folder = attachments_dir / safe_id;
    fs::create_directories(folder, ec);
    std::string name = sanitize_name(base);
    fs::path target = folder / make_path(name);
    std::string stem = name;
    std::string ext;
    auto dot = name.find_last_of('.');
    if (dot != std::string::npos) {
        stem = name.substr(0, dot);
        ext = name.substr(dot);
    }
    int counter = 1;
    while (fs::exists(target, ec)) {
        target = folder / make_path(stem + "-" + std::to_string(counter++) + ext);
    }
    fs::copy_file(src, target, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        throw StoreError("复制附件失败：" + ec.message());
    }
    Attachment att;
    att.path = "attachments/" + safe_id + "/" +
#ifdef _WIN32
               to_utf8(target.filename().wstring());
#else
               target.filename().string();
#endif
    att.name =
#ifdef _WIN32
        to_utf8(target.filename().wstring());
#else
        target.filename().string();
#endif
    att.mime = guess_mime(att.name);
    att.size = (long long)fs::file_size(target, ec);
    att.added_at = now_iso();
    return att;
}

Attachment Store::add_attachment_bytes(const std::string& event_id,
                                       const std::string& name, const Bytes& bytes) {
    std::string safe_id = is_safe_segment(event_id) ? event_id : "misc";
    fs::path folder = attachments_dir / safe_id;
    std::error_code ec;
    fs::create_directories(folder, ec);
    std::string safe = sanitize_name(name);
    fs::path target = folder / make_path(safe);
    std::string stem = safe;
    std::string ext;
    auto dot = safe.find_last_of('.');
    if (dot != std::string::npos) {
        stem = safe.substr(0, dot);
        ext = safe.substr(dot);
    }
    int counter = 1;
    while (fs::exists(target, ec)) {
        target = folder / make_path(stem + "-" + std::to_string(counter++) + ext);
    }
    if (!write_bytes_atomic(target, bytes)) {
        throw StoreError("写入附件失败");
    }
    Attachment att;
    att.path = "attachments/" + safe_id + "/" +
#ifdef _WIN32
               to_utf8(target.filename().wstring());
#else
               target.filename().string();
#endif
    att.name =
#ifdef _WIN32
        to_utf8(target.filename().wstring());
#else
        target.filename().string();
#endif
    att.mime = guess_mime(att.name);
    att.size = (long long)bytes.size();
    att.added_at = now_iso();
    return att;
}

fs::path Store::resolve(const std::string& rel) const {
    fs::path rel_path =
#ifdef _WIN32
        fs::path(to_wide(rel));
#else
        fs::path(rel);
#endif
    std::error_code ec;
    fs::path full = fs::weakly_canonical(dir / rel_path, ec);
    if (ec) {
        full = dir / rel_path;
    }
    fs::path base = fs::weakly_canonical(dir, ec);
    auto base_str =
#ifdef _WIN32
        base.wstring();
#else
        base.string();
#endif
    auto full_str =
#ifdef _WIN32
        full.wstring();
#else
        full.string();
#endif
    if (full_str.size() < base_str.size() ||
        full_str.compare(0, base_str.size(), base_str) != 0) {
        throw StoreError("附件路径越界：" + rel);
    }
    return full;
}

bool Store::attachment_exists(const Attachment& att) const {
    try {
        return fs::is_regular_file(resolve(att));
    } catch (...) {
        return false;
    }
}

void Store::remove_attachments(const std::string& event_id) const {
    std::error_code ec;
    fs::remove_all(attachments_dir / event_id, ec);
}

int Store::gc_orphans(const std::vector<std::string>& keep_ids) const {
    int removed = 0;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(attachments_dir, ec)) {
        if (!entry.is_directory()) {
            continue;
        }
        std::string name =
#ifdef _WIN32
            to_utf8(entry.path().filename().wstring());
#else
            entry.path().filename().string();
#endif
        if (std::find(keep_ids.begin(), keep_ids.end(), name) == keep_ids.end()) {
            fs::remove_all(entry.path(), ec);
            ++removed;
        }
    }
    return removed;
}

void Store::snapshot(const std::string& tag, int keep) const {
    if (!fs::exists(events_path)) {
        return;
    }
    std::string text = read_file(events_path);
    if (trim(text).empty()) {
        return;
    }
    fs::path target = backups_dir / (tag + "-" + timestamp() + ".json");
    write_file_atomic(target, text);
    auto files = backup_files(tag + "-");
    int excess = (int)files.size() - keep;
    std::error_code ec;
    for (int i = 0; i < excess && i < (int)files.size(); ++i) {
        fs::remove(files[(size_t)i], ec);
    }
}

bool Store::restore_last(const std::string& tag) {
    auto files = backup_files(tag + "-");
    while (!files.empty()) {
        fs::path latest = files.back();
        files.pop_back();
        std::string text = read_file(latest);
        if (trim(text).empty()) {
            continue;
        }
        try {
            json parsed = json::parse(text);
            (void)parsed;
        } catch (...) {
            continue;
        }
        if (!write_file_atomic(events_path, text)) {
            return false;
        }
        write_file_atomic(backup_path, text);
        std::error_code ec;
        fs::remove(latest, ec);
        return true;
    }
    return false;
}

bool Store::has_snapshots(const std::string& tag) const {
    return !backup_files(tag + "-").empty();
}

void Store::clear_snapshots(const std::string& tag) const {
    std::error_code ec;
    for (const auto& path : backup_files(tag + "-")) {
        fs::remove(path, ec);
    }
}

std::optional<fs::file_time_type> Store::events_mtime() const {
    std::error_code ec;
    if (!fs::exists(events_path, ec)) {
        return std::nullopt;
    }
    auto t = fs::last_write_time(events_path, ec);
    if (ec) {
        return std::nullopt;
    }
    return t;
}

std::string Store::display_dir() const {
#ifdef _WIN32
    return to_utf8(dir.wstring());
#else
    return dir.string();
#endif
}

std::string Store::info_text() const {
    std::ostringstream ss;
    ss << "数据目录：" << display_dir() << "\n";
    ss << "事件文件：" << display_dir() << "/events.json\n";
    return ss.str();
}

}  // namespace moji

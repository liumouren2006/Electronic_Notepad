#include "importer.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <map>
#include <set>

#include "zip.hpp"

namespace moji {

namespace {
Bytes read_bytes(const fs::path& path, bool& ok) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        ok = false;
        return {};
    }
    in.seekg(0, std::ios::end);
    std::streamoff size = in.tellg();
    in.seekg(0, std::ios::beg);
    if (size < 0) {
        ok = false;
        return {};
    }
    Bytes out((size_t)size);
    if (size > 0) {
        in.read(reinterpret_cast<char*>(out.data()), size);
    }
    ok = (bool)in;
    return out;
}

std::string base_name(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string safe_zip_name(std::string name) {
    for (char& c : name) {
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|' || (unsigned char)c < 32) {
            c = '_';
        }
    }
    while (!name.empty() && name.front() == '.') {
        name.erase(name.begin());
    }
    return name.empty() ? "file" : name;
}

char priority_letter(int p) {
    int idx = std::max(0, std::min(25, p - 1));
    return (char)('A' + idx);
}

bool looks_like_date(const std::string& s) {
    if (s.size() != 10) {
        return false;
    }
    return s[4] == '-' && s[7] == '-' && parse_datetime(s).has_value();
}

std::string one_line(std::string s) {
    for (char& c : s) {
        if (c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    return s;
}
}  // namespace

void export_json(Repository& repo, const fs::path& path) {
    json payload;
    payload["app"] = "moji-note";
    payload["version"] = 1;
    payload["exported_at"] = now_iso();
    payload["events"] = json::array();
    for (const Event& e : repo.events) {
        payload["events"].push_back(e.to_json());
    }
    payload["count"] = payload["events"].size();
    if (!write_file_atomic(path, payload.dump(2, ' ', false))) {
        throw RepoError("导出失败：无法写入文件");
    }
}

ImportReport import_json(Repository& repo, const fs::path& path, bool merge) {
    bool ok = false;
    std::string text = read_file(path, &ok);
    if (!ok) {
        throw RepoError("无法读取文件");
    }
    json data;
    try {
        data = json::parse(text);
    } catch (const std::exception& e) {
        throw RepoError(std::string("JSON 格式错误：") + e.what());
    }
    json arr = json::array();
    if (data.is_object() && data.contains("events") && data["events"].is_array()) {
        arr = data["events"];
    } else if (data.is_array()) {
        arr = data;
    } else {
        throw RepoError("文件格式不正确：缺少 events 数组");
    }
    std::vector<Event> incoming;
    for (const auto& item : arr) {
        try {
            Event e = Event::from_json(item);
            if (!e.title.empty()) {
                incoming.push_back(std::move(e));
            }
        } catch (...) {
        }
    }
    return repo.import_events(std::move(incoming), merge);
}

void export_todotxt(Repository& repo, const fs::path& path) {
    std::string out;
    for (const Event& e : repo.events) {
        std::vector<std::string> parts;
        if (e.done) {
            parts.push_back("x");
            parts.push_back(date_str(e.done_at.value_or(now_time())));
        }
        parts.push_back(std::string("(") + priority_letter(e.priority) + ")");
        if (e.created_at.has_value()) {
            parts.push_back(date_str(*e.created_at));
        }
        parts.push_back(one_line(e.title));
        if (e.due.has_value()) {
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &*e.due);
#else
            localtime_r(&*e.due, &tm);
#endif
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M", &tm);
            parts.push_back(std::string("due:") + buf);
        }
        for (const auto& tag : e.tags) {
            std::string t = tag;
            std::replace(t.begin(), t.end(), ' ', '_');
            parts.push_back("+" + t);
        }
        out += join(parts, " ") + "\n";
    }
    if (!write_file_atomic(path, out)) {
        throw RepoError("导出失败：无法写入文件");
    }
}

ImportReport import_todotxt(Repository& repo, const fs::path& path, bool merge) {
    bool ok = false;
    std::string text = read_file(path, &ok);
    if (!ok) {
        throw RepoError("无法读取文件");
    }
    std::vector<Event> incoming;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t end = text.find('\n', pos);
        std::string line = text.substr(pos, end == std::string::npos ? std::string::npos
                                                                     : end - pos);
        if (end == std::string::npos) {
            pos = text.size() + 1;
        } else {
            pos = end + 1;
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        bool done = false;
        if (line.rfind("x ", 0) == 0) {
            done = true;
            line = trim(line.substr(2));
        }
        std::vector<std::string> tokens;
        {
            std::string cur;
            for (char c : line) {
                if (c == ' ' || c == '\t') {
                    if (!cur.empty()) {
                        tokens.push_back(cur);
                        cur.clear();
                    }
                } else {
                    cur.push_back(c);
                }
            }
            if (!cur.empty()) {
                tokens.push_back(cur);
            }
        }
        int priority = 1;
        std::string title;
        std::vector<std::string> tags;
        std::optional<std::time_t> due;
        std::optional<std::time_t> created;
        std::optional<std::time_t> completed;
        for (const auto& tok : tokens) {
            if (tok.size() == 3 && tok.front() == '(' && tok.back() == ')') {
                char c = (char)std::toupper((unsigned char)tok[1]);
                if (c >= 'A' && c <= 'Z') {
                    priority = c - 'A' + 1;
                    continue;
                }
            }
            if (tok.rfind("due:", 0) == 0) {
                due = parse_datetime(tok.substr(4));
                continue;
            }
            if (tok.size() > 2 && tok[0] == 't' && tok[1] == ':') {
                continue;
            }
            if (tok.size() > 1 && tok[0] == '+') {
                std::string tag = tok.substr(1);
                std::replace(tag.begin(), tag.end(), '_', ' ');
                tags.push_back(tag);
                continue;
            }
            if (looks_like_date(tok)) {
                auto d = parse_datetime(tok);
                if (!created.has_value()) {
                    created = d;
                } else if (!completed.has_value()) {
                    completed = d;
                }
                continue;
            }
            if (!title.empty()) {
                title += " ";
            }
            title += tok;
        }
        if (title.empty()) {
            continue;
        }
        Event e = Event::create(title, "", due, tags, 0, "");
        e.priority = priority;
        e.done = done;
        e.created_at = created.value_or(now_time());
        e.updated_at = e.created_at;
        if (done) {
            e.done_at = completed.value_or(now_time());
        }
        incoming.push_back(std::move(e));
    }
    return repo.import_events(std::move(incoming), merge);
}

void export_markdown(Repository& repo, const fs::path& path) {
    Stats stats = repo.stats();
    std::string out = "# 墨记 · 记事本导出\n\n";
    out += "> 导出时间：" + iso_from_time(now_time()) + "　共 " +
           std::to_string(stats.total) + " 条\n\n";
    auto dump_group = [&](const std::string& title, std::vector<Event*> list) {
        out += "## " + title + "（" + std::to_string(list.size()) + "）\n\n";
        if (list.empty()) {
            out += "_暂无_\n\n";
            return;
        }
        for (Event* e : list) {
            out += "### " + std::to_string(e->priority) + ". [" +
                   (e->done ? "x" : " ") + "] " + e->title + "\n";
            out += "- 截止：" + (e->due.has_value() ? humanize_due(e->due) : "无期限") + "\n";
            if (!e->tags.empty()) {
                out += "- 标签：";
                for (const auto& t : e->tags) {
                    out += "`#" + t + "` ";
                }
                out += "\n";
            }
            if (!e->attachments.empty()) {
                out += "- 附件：";
                for (size_t i = 0; i < e->attachments.size(); ++i) {
                    if (i) {
                        out += "、";
                    }
                    out += e->attachments[i].name + "（" + e->attachments[i].mime + "）";
                }
                out += "\n";
            }
            if (!e->note.empty()) {
                out += "\n";
                for (const auto& line : split(e->note, '\n')) {
                    out += "> " + line + "\n";
                }
            }
            out += "\n";
        }
    };
    dump_group("进行中", repo.pending());
    dump_group("已完成", repo.finished());
    if (!write_file_atomic(path, out)) {
        throw RepoError("导出失败：无法写入文件");
    }
}

fs::path export_bundle(Repository& repo, const fs::path& path_in) {
    fs::path path = path_in;
    if (path.extension() != ".mjnote") {
        path.replace_extension(".mjnote");
    }
    json manifest;
    manifest["app"] = "moji-note";
    manifest["version"] = 2;
    manifest["exported_at"] = now_iso();
    manifest["events"] = json::array();

    std::vector<std::pair<std::string, Bytes>> entries;
    for (const Event& e : repo.events) {
        json item = e.to_json();
        item["attachments"] = json::array();
        for (size_t i = 0; i < e.attachments.size(); ++i) {
            const Attachment& att = e.attachments[i];
            fs::path file;
            try {
                file = repo.store.resolve(att);
            } catch (...) {
                continue;
            }
            bool ok = false;
            Bytes data = read_bytes(file, ok);
            if (!ok) {
                continue;
            }
            std::string member = "files/" + e.id + "/" + std::to_string(i) + "_" +
                                 safe_zip_name(att.name);
            json aj = att.to_json();
            aj["src"] = member;
            item["attachments"].push_back(aj);
            entries.emplace_back(member, std::move(data));
        }
        manifest["events"].push_back(item);
    }
    manifest["count"] = manifest["events"].size();
    std::string text = manifest.dump(2, ' ', false);
    entries.insert(entries.begin(),
                   {"manifest.json", Bytes(text.begin(), text.end())});
    Bytes zipped = zip_write(entries);
    if (!write_bytes_atomic(path, zipped)) {
        throw RepoError("打包失败：无法写入文件");
    }
    return path;
}

ImportReport import_bundle(Repository& repo, const fs::path& path, bool merge) {
    bool ok = false;
    Bytes data = read_bytes(path, ok);
    if (!ok) {
        throw RepoError("无法读取打包文件");
    }
    std::map<std::string, Bytes> files;
    try {
        files = zip_read(data);
    } catch (const std::exception& e) {
        throw RepoError(std::string("打包文件损坏：") + e.what());
    }
    auto it = files.find("manifest.json");
    if (it == files.end()) {
        throw RepoError("打包文件缺少 manifest.json");
    }
    json manifest;
    try {
        std::string text(it->second.begin(), it->second.end());
        manifest = json::parse(text);
    } catch (const std::exception& e) {
        throw RepoError(std::string("清单格式错误：") + e.what());
    }
    if (!manifest.contains("events") || !manifest["events"].is_array()) {
        throw RepoError("清单缺少事件数组");
    }
    std::vector<Event> incoming;
    for (const auto& item : manifest["events"]) {
        try {
            Event e = Event::from_json(item);
            if (e.title.empty()) {
                continue;
            }
            e.attachments.clear();
            if (item.contains("attachments") && item["attachments"].is_array()) {
                for (const auto& aj : item["attachments"]) {
                    if (!aj.is_object() || !aj.contains("src") || !aj["src"].is_string()) {
                        continue;
                    }
                    std::string member = aj["src"].get<std::string>();
                    auto fit = files.find(member);
                    if (fit == files.end()) {
                        continue;
                    }
                    std::string name = aj.value("name", base_name(member));
                    e.pending_attachments.emplace_back(name, fit->second);
                }
            }
            incoming.push_back(std::move(e));
        } catch (...) {
        }
    }
    auto fill = [&repo](Event& e) {
        for (auto& pending : e.pending_attachments) {
            Attachment att =
                repo.store.add_attachment_bytes(e.id, pending.first, pending.second);
            e.attachments.push_back(att);
        }
        e.pending_attachments.clear();
    };
    return repo.import_events(std::move(incoming), merge, fill);
}

fs::path export_auto(Repository& repo, const fs::path& path) {
    std::string ext = lower_ascii(path.extension().string());
    if (ext == ".mjnote") {
        return export_bundle(repo, path);
    }
    if (ext == ".txt") {
        export_todotxt(repo, path);
        return path;
    }
    if (ext == ".md" || ext == ".markdown") {
        export_markdown(repo, path);
        return path;
    }
    export_json(repo, path);
    return path;
}

ImportReport import_auto(Repository& repo, const fs::path& path, bool merge) {
    std::string ext = lower_ascii(path.extension().string());
    if (ext == ".mjnote") {
        return import_bundle(repo, path, merge);
    }
    if (ext == ".txt") {
        return import_todotxt(repo, path, merge);
    }
    if (ext == ".md" || ext == ".markdown") {
        throw RepoError("暂不支持 Markdown 反向导入，请使用 .json / .mjnote / .txt");
    }
    return import_json(repo, path, merge);
}

}  // namespace moji

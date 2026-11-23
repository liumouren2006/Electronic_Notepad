#include "model.hpp"

#include <algorithm>

namespace moji {

namespace {
std::optional<std::time_t> opt_time(const json& j, const char* key) {
    if (!j.contains(key) || !j[key].is_string()) {
        return std::nullopt;
    }
    return parse_datetime(j[key].get<std::string>());
}

std::string time_str(const std::optional<std::time_t>& t) {
    return t.has_value() ? iso_from_time(*t) : "";
}
}  // namespace

json Attachment::to_json() const {
    return json{{"path", path},
                {"name", name},
                {"mime", mime},
                {"size", size},
                {"added_at", added_at}};
}

Attachment Attachment::from_json(const json& j) {
    Attachment a;
    if (!j.is_object()) {
        return a;
    }
    a.path = j.value("path", "");
    a.name = j.value("name", "");
    a.mime = j.value("mime", "");
    a.size = j.value("size", 0LL);
    a.added_at = j.value("added_at", "");
    if (a.name.empty() && !a.path.empty()) {
        auto pos = a.path.find_last_of('/');
        a.name = pos == std::string::npos ? a.path : a.path.substr(pos + 1);
    }
    if (a.mime.empty()) {
        a.mime = guess_mime(a.name);
    }
    return a;
}

Event Event::create(const std::string& title, const std::string& note,
                    std::optional<std::time_t> due, std::vector<std::string> tags,
                    int remind_before, const std::string& color) {
    Event e;
    e.id = uuid8();
    e.title = trim(title);
    e.note = note;
    e.due = due;
    e.tags = tags;
    e.remind_before = std::max(0, remind_before);
    e.color = color;
    e.created_at = now_time();
    e.updated_at = e.created_at;
    return e;
}

json Event::to_json() const {
    json tags_json = json::array();
    for (const auto& t : tags) {
        tags_json.push_back(t);
    }
    json att_json = json::array();
    for (const auto& a : attachments) {
        att_json.push_back(a.to_json());
    }
    return json{{"id", id},
                {"title", title},
                {"note", note},
                {"due", time_str(due)},
                {"priority", priority},
                {"done", done},
                {"done_at", time_str(done_at)},
                {"tags", tags_json},
                {"attachments", att_json},
                {"created_at", time_str(created_at)},
                {"updated_at", time_str(updated_at)},
                {"remind_before", remind_before},
                {"last_reminded_at", time_str(last_reminded_at)},
                {"color", color}};
}

Event Event::from_json(const json& j) {
    Event e;
    if (!j.is_object()) {
        return e;
    }
    e.id = j.value("id", "");
    e.title = trim(j.value("title", ""));
    e.note = j.value("note", "");
    e.due = opt_time(j, "due");
    e.priority = std::max(1, j.value("priority", 1));
    e.done = j.value("done", false);
    e.done_at = opt_time(j, "done_at");
    if (j.contains("tags")) {
        if (j["tags"].is_array()) {
            for (const auto& t : j["tags"]) {
                if (t.is_string()) {
                    e.tags.push_back(t.get<std::string>());
                }
            }
        } else if (j["tags"].is_string()) {
            e.tags = split_tags(j["tags"].get<std::string>());
        }
    }
    if (j.contains("attachments") && j["attachments"].is_array()) {
        for (const auto& a : j["attachments"]) {
            if (a.is_object() && a.contains("path") && a["path"].is_string() &&
                !a["path"].get<std::string>().empty()) {
                e.attachments.push_back(Attachment::from_json(a));
            }
        }
    }
    e.created_at = opt_time(j, "created_at");
    if (!e.created_at.has_value()) {
        e.created_at = now_time();
    }
    e.updated_at = opt_time(j, "updated_at");
    if (!e.updated_at.has_value()) {
        e.updated_at = e.created_at;
    }
    e.remind_before = std::max(0, j.value("remind_before", 0));
    e.last_reminded_at = opt_time(j, "last_reminded_at");
    e.color = j.value("color", "");
    if (e.done && !e.done_at.has_value()) {
        e.done_at = now_time();
    }
    if (!e.done) {
        e.done_at.reset();
    }
    if (e.id.empty()) {
        e.id = uuid8();
    }
    return e;
}

bool Event::is_overdue(std::optional<std::time_t> at) const {
    if (!due.has_value() || done) {
        return false;
    }
    return *due < at.value_or(now_time());
}

bool Event::is_due_on(std::time_t day) const {
    return due.has_value() && same_day(*due, day);
}

bool Event::is_due_today() const { return is_due_on(now_time()); }

std::optional<std::time_t> Event::remind_at() const {
    if (!due.has_value()) {
        return std::nullopt;
    }
    return *due - (std::time_t)remind_before * 60;
}

bool Event::needs_reminder(std::time_t at) const {
    if (done || !due.has_value()) {
        return false;
    }
    auto moment = remind_at();
    if (!moment.has_value() || at < *moment) {
        return false;
    }
    if (last_reminded_at.has_value() && *last_reminded_at >= *moment) {
        return false;
    }
    return true;
}

std::string Event::display_title() const {
    return (done ? "✓ " : "") + title;
}

std::string Event::badge() const {
    if (done) {
        return "已完成";
    }
    if (is_overdue()) {
        return "逾期";
    }
    if (is_due_today()) {
        return "今天";
    }
    return "P" + std::to_string(priority);
}

std::string Event::due_text() const { return humanize_due(due); }

}  // namespace moji

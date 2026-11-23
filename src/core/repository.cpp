#include "repository.hpp"

#include <algorithm>
#include <set>

namespace moji {

Repository::Repository(const fs::path& data_dir) : store(data_dir) { reload(); }

void Repository::reload() {
    auto raw = store.load_events();
    std::vector<Event> parsed;
    warnings = store.warnings;
    for (const auto& item : raw) {
        try {
            Event e = Event::from_json(item);
            if (e.title.empty()) {
                throw std::runtime_error("标题为空");
            }
            parsed.push_back(std::move(e));
        } catch (const std::exception& e) {
            warnings.push_back(std::string("跳过损坏记录：") + e.what());
        }
    }
    std::stable_sort(parsed.begin(), parsed.end(), [](const Event& a, const Event& b) {
        if (a.priority != b.priority) {
            return a.priority < b.priority;
        }
        return a.created_at.value_or(0) < b.created_at.value_or(0);
    });
    events.load(std::move(parsed));
    events.reindex();
}

void Repository::save() { store.save_events(events.to_json_array()); }

std::vector<json> Repository::snapshot() const { return events.to_json_array(); }

void Repository::push_undo() {
    undo_.push_back(snapshot());
    if (undo_.size() > kMaxUndo) {
        undo_.erase(undo_.begin());
    }
    redo_.clear();
}

void Repository::restore(const std::vector<json>& snap) {
    std::vector<Event> parsed;
    for (const auto& item : snap) {
        try {
            parsed.push_back(Event::from_json(item));
        } catch (...) {
        }
    }
    events.load(std::move(parsed));
    events.reindex();
}

Event& Repository::require(const std::string& id) {
    Event* e = events.find(id);
    if (!e) {
        throw RepoError("找不到事件：" + id);
    }
    return *e;
}

Event& Repository::add(const std::string& title, const std::string& note,
                       std::optional<std::time_t> due, std::vector<std::string> tags,
                       std::optional<int> position, int remind_before,
                       const std::string& color) {
    std::string clean = trim(title);
    if (clean.empty()) {
        throw RepoError("标题不能为空");
    }
    push_undo();
    Event e = Event::create(clean, note, due, tags, remind_before, color);
    Event* added = nullptr;
    if (position.has_value()) {
        added = &events.insert_at(*position, std::move(e));
    } else {
        added = &events.append(std::move(e));
    }
    added->updated_at = now_time();
    events.reindex();
    save();
    return *added;
}

Event& Repository::update(const std::string& id, const std::string& title,
                          const std::string& note, std::optional<std::time_t> due,
                          std::vector<std::string> tags, int remind_before,
                          std::optional<std::string> color,
                          std::optional<std::string> icon,
                          std::optional<std::string> page_layout,
                          std::optional<bool> system_notify) {
    Event& e = require(id);
    std::string clean = trim(title);
    if (clean.empty()) {
        throw RepoError("标题不能为空");
    }
    push_undo();
    e.title = clean;
    e.note = note;
    e.due = due;
    e.tags = tags;
    e.remind_before = std::max(0, remind_before);
    if (system_notify.has_value()) {
        e.system_notify = *system_notify;
    }
    if (color.has_value()) {
        e.color = *color;
    }
    if (icon.has_value()) {
        e.icon = *icon;
    }
    if (page_layout.has_value()) {
        e.page_layout = *page_layout;
    }
    e.updated_at = now_time();
    e.last_reminded_at.reset();
    save();
    return e;
}

Event& Repository::set_visual(const std::string& id,
                              std::optional<std::string> color,
                              std::optional<std::string> icon,
                              std::optional<std::string> page_layout) {
    Event& e = require(id);
    push_undo();
    if (color.has_value()) e.color = *color;
    if (icon.has_value()) e.icon = *icon;
    if (page_layout.has_value()) e.page_layout = *page_layout;
    e.updated_at = now_time();
    save();
    return e;
}

Event& Repository::set_done(const std::string& id, bool done) {
    Event& e = require(id);
    push_undo();
    e.done = done;
    e.done_at = done ? std::optional<std::time_t>(now_time()) : std::nullopt;
    e.updated_at = now_time();
    save();
    return e;
}

Event& Repository::toggle_done(const std::string& id) {
    Event& e = require(id);
    return set_done(id, !e.done);
}

bool Repository::remove(const std::string& id) {
    if (!events.find(id)) {
        return false;
    }
    push_undo();
    events.remove(id);
    events.reindex();
    save();
    return true;
}

int Repository::clear_done() {
    std::vector<std::string> ids;
    for (Event& e : events) {
        if (e.done) {
            ids.push_back(e.id);
        }
    }
    if (ids.empty()) {
        return 0;
    }
    push_undo();
    for (const auto& id : ids) {
        events.remove(id);
    }
    events.reindex();
    save();
    return (int)ids.size();
}

Event& Repository::duplicate(const std::string& id) {
    Event& src = require(id);
    push_undo();
    Event copy = src;
    copy.id = uuid8();
    copy.title = src.title + "（副本）";
    copy.done = false;
    copy.done_at.reset();
    copy.created_at = now_time();
    copy.updated_at = *copy.created_at;
    copy.last_reminded_at.reset();
    int pos = events.index_of(id);
    Event& added = events.insert_at(pos + 1, std::move(copy));
    events.reindex();
    save();
    return added;
}

bool Repository::move_to(const std::string& id, int pos) {
    if (!events.find(id)) {
        return false;
    }
    push_undo();
    bool ok = events.move_to(id, pos);
    if (ok) {
        save();
    }
    return ok;
}

bool Repository::move_by(const std::string& id, int delta) {
    if (!events.find(id)) {
        return false;
    }
    push_undo();
    bool ok = events.move_by(id, delta);
    if (ok) {
        save();
    }
    return ok;
}

Event* Repository::find(const std::string& id) { return events.find(id); }
const Event* Repository::find(const std::string& id) const { return events.find(id); }

std::vector<Event*> Repository::all() { return events.views(); }

std::vector<Event*> Repository::pending() {
    std::vector<Event*> out;
    for (Event* e : events.views()) {
        if (!e->done) {
            out.push_back(e);
        }
    }
    return out;
}

std::vector<Event*> Repository::finished() {
    std::vector<Event*> out;
    for (Event* e : events.views()) {
        if (e->done) {
            out.push_back(e);
        }
    }
    return out;
}

std::vector<Event*> Repository::overdue(std::optional<std::time_t> at) {
    std::time_t moment = at.value_or(now_time());
    std::vector<Event*> out;
    for (Event* e : events.views()) {
        if (e->is_overdue(moment)) {
            out.push_back(e);
        }
    }
    return out;
}

std::vector<Event*> Repository::due_on(std::time_t day) {
    std::vector<Event*> out;
    for (Event* e : events.views()) {
        if (e->is_due_on(day)) {
            out.push_back(e);
        }
    }
    return out;
}

std::vector<Event*> Repository::due_reminders(std::time_t at) {
    std::vector<Event*> out;
    for (Event* e : events.views()) {
        if (e->needs_reminder(at)) {
            out.push_back(e);
        }
    }
    return out;
}

std::vector<Event*> Repository::search(const std::string& keyword) {
    std::string key = lower_ascii(trim(keyword));
    if (key.empty()) {
        return events.views();
    }
    std::vector<std::string> terms;
    {
        std::string cur;
        for (char c : key) {
            if (c == ' ' || c == '\t') {
                if (!cur.empty()) {
                    terms.push_back(cur);
                    cur.clear();
                }
            } else {
                cur.push_back(c);
            }
        }
        if (!cur.empty()) {
            terms.push_back(cur);
        }
    }
    std::vector<Event*> out;
    for (Event* e : events.views()) {
        std::string hay = lower_ascii(e->title + " " + e->note + " " + join(e->tags, " "));
        for (const auto& a : e->attachments) {
            hay += " " + lower_ascii(a.name);
        }
        bool all = true;
        for (const auto& term : terms) {
            if (hay.find(term) == std::string::npos) {
                all = false;
                break;
            }
        }
        if (all) {
            out.push_back(e);
        }
    }
    return out;
}

std::map<std::string, int> Repository::tags() const {
    std::map<std::string, int> counts;
    for (const Event& e : events) {
        if (e.done) {
            continue;
        }
        for (const auto& t : e.tags) {
            counts[t]++;
        }
    }
    return counts;
}

Stats Repository::stats(std::optional<std::time_t> at) const {
    std::time_t moment = at.value_or(now_time());
    Stats s;
    s.total = (int)events.size();
    for (const Event& e : events) {
        if (e.done) {
            s.done++;
        } else {
            s.pending++;
            if (e.due.has_value() && *e.due < moment) {
                s.overdue++;
            }
        }
        if (e.is_due_on(moment)) {
            s.today++;
        }
        s.attachments += (int)e.attachments.size();
    }
    s.rate = s.total ? (int)((double)s.done * 100.0 / s.total + 0.5) : 0;
    return s;
}

void Repository::mark_reminded(std::vector<Event*> list, std::time_t at) {
    if (list.empty()) {
        return;
    }
    for (Event* e : list) {
        e->last_reminded_at = at;
    }
    save();
}

Attachment& Repository::add_attachment(const std::string& event_id, const fs::path& src) {
    Event& e = require(event_id);
    Attachment att = store.add_attachment(event_id, src);
    push_undo();
    e.attachments.push_back(att);
    e.updated_at = now_time();
    save();
    return e.attachments.back();
}

void Repository::remove_attachment(const std::string& event_id, int index) {
    Event& e = require(event_id);
    if (index < 0 || index >= (int)e.attachments.size()) {
        throw RepoError("附件序号无效");
    }
    push_undo();
    e.attachments.erase(e.attachments.begin() + index);
    e.updated_at = now_time();
    save();
}

void Repository::reorder_attachments(const std::string& event_id,
                                     const std::vector<std::string>& rel_paths) {
    Event& e = require(event_id);
    if (rel_paths.size() != e.attachments.size()) {
        throw RepoError("附件顺序与现有附件数量不一致");
    }
    std::map<std::string, Attachment> by_path;
    for (const auto& a : e.attachments) {
        by_path[a.path] = a;
    }
    std::vector<Attachment> ordered;
    for (const auto& p : rel_paths) {
        auto it = by_path.find(p);
        if (it == by_path.end()) {
            throw RepoError("附件顺序与现有附件不一致");
        }
        ordered.push_back(it->second);
        by_path.erase(it);
    }
    push_undo();
    e.attachments = ordered;
    e.updated_at = now_time();
    save();
}

bool Repository::undo() {
    if (undo_.empty()) {
        return false;
    }
    redo_.push_back(snapshot());
    auto snap = undo_.back();
    undo_.pop_back();
    restore(snap);
    save();
    return true;
}

bool Repository::redo() {
    if (redo_.empty()) {
        return false;
    }
    undo_.push_back(snapshot());
    auto snap = redo_.back();
    redo_.pop_back();
    restore(snap);
    save();
    return true;
}

ImportReport Repository::import_events(
    std::vector<Event> incoming, bool merge,
    const std::function<void(Event&)>& fill_attachments) {
    ImportReport report;
    std::set<std::string> ids;
    std::set<std::string> fingerprints;
    for (const Event& e : events) {
        ids.insert(e.id);
        fingerprints.insert(e.title + "\x1f" + std::to_string(e.created_at.value_or(0)));
    }
    push_undo();
    if (!merge) {
        events.clear();
        ids.clear();
        fingerprints.clear();
    }
    for (auto& e : incoming) {
        if (e.title.empty()) {
            report.skipped++;
            continue;
        }
        std::string fp = e.title + "\x1f" + std::to_string(e.created_at.value_or(0));
        if (merge && (ids.count(e.id) || fingerprints.count(fp))) {
            report.skipped++;
            continue;
        }
        if (ids.count(e.id) || e.id.empty()) {
            e.id = uuid8();
        }
        if (fill_attachments) {
            try {
                fill_attachments(e);
            } catch (...) {
            }
        }
        ids.insert(e.id);
        fingerprints.insert(fp);
        e.done_at = e.done ? e.done_at : std::nullopt;
        events.append(std::move(e));
        report.added++;
    }
    events.reindex();
    save();
    return report;
}

int Repository::gc_orphans() {
    std::vector<std::string> keep;
    for (const Event& e : events) {
        keep.push_back(e.id);
    }
    return store.gc_orphans(keep);
}

}  // namespace moji

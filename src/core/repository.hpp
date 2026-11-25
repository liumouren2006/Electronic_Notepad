// 业务仓储：增删改查、优先级、撤销、附件、查询
#pragma once

#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "elist.hpp"
#include "store.hpp"

namespace moji {

class RepoError : public std::runtime_error {
public:
    explicit RepoError(const std::string& msg) : std::runtime_error(msg) {}
};

struct Stats {
    int total = 0;
    int pending = 0;
    int done = 0;
    int today = 0;
    int overdue = 0;
    int attachments = 0;
    int rate = 0;
};

struct ImportReport {
    int added = 0;
    int skipped = 0;
};

class Repository {
public:
    explicit Repository(const fs::path& data_dir = {});

    Store store;
    EventList events;
    std::vector<std::string> warnings;

    void reload();
    void save();

    Event& add(const std::string& title, const std::string& note = "",
               std::optional<std::time_t> due = std::nullopt,
               std::vector<std::string> tags = {},
               std::optional<int> position = std::nullopt, int remind_before = 0,
               const std::string& color = "");
    Event& update(const std::string& id, const std::string& title,
                  const std::string& note, std::optional<std::time_t> due,
                  std::vector<std::string> tags, int remind_before,
                  std::optional<std::string> color = std::nullopt,
                  std::optional<std::string> icon = std::nullopt,
                  std::optional<std::string> page_layout = std::nullopt,
                  std::optional<bool> system_notify = std::nullopt);
    Event& set_visual(const std::string& id, std::optional<std::string> color,
                      std::optional<std::string> icon = std::nullopt,
                      std::optional<std::string> page_layout = std::nullopt);
    Event& set_ringtone(const std::string& id, const std::string& path);
    Event& set_snooze(const std::string& id, std::optional<std::time_t> until);
    std::optional<std::time_t> next_reminder_at(std::time_t after) const;
    Event& set_done(const std::string& id, bool done);
    Event& toggle_done(const std::string& id);
    bool remove(const std::string& id);
    int clear_done();
    Event& duplicate(const std::string& id);
    bool move_to(const std::string& id, int pos);
    bool move_by(const std::string& id, int delta);

    Event* find(const std::string& id);
    const Event* find(const std::string& id) const;
    std::vector<Event*> all();
    std::vector<Event*> pending();
    std::vector<Event*> finished();
    std::vector<Event*> overdue(std::optional<std::time_t> at = std::nullopt);
    std::vector<Event*> due_on(std::time_t day);
    std::vector<Event*> due_reminders(std::time_t at);
    std::vector<Event*> search(const std::string& keyword);
    std::map<std::string, int> tags() const;
    Stats stats(std::optional<std::time_t> at = std::nullopt) const;

    void mark_reminded(std::vector<Event*> list, std::time_t at);

    Attachment& add_attachment(const std::string& event_id, const fs::path& src);
    void remove_attachment(const std::string& event_id, int index);
    void reorder_attachments(const std::string& event_id,
                             const std::vector<std::string>& rel_paths);

    bool undo();
    bool redo();
    bool can_undo() const { return !undo_.empty(); }
    bool can_redo() const { return !redo_.empty(); }

    ImportReport import_events(
        std::vector<Event> incoming, bool merge,
        const std::function<void(Event&)>& fill_attachments = {});

    int gc_orphans();
    std::optional<fs::file_time_type> mtime() const { return store.events_mtime(); }
    std::string display_dir() const { return store.display_dir(); }

private:
    std::vector<std::vector<json>> undo_;
    std::vector<std::vector<json>> redo_;
    static constexpr size_t kMaxUndo = 60;

    std::vector<json> snapshot() const;
    void push_undo();
    void restore(const std::vector<json>& snap);
    Event& require(const std::string& id);
};

}  // namespace moji

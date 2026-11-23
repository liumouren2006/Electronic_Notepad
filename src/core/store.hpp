// 持久化：UTF-8 JSON 文本 + 附件目录 + 备份 / 快照
#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "model.hpp"

namespace moji {

class StoreError : public std::runtime_error {
public:
    explicit StoreError(const std::string& msg) : std::runtime_error(msg) {}
};

class Store {
public:
    explicit Store(const fs::path& data_dir = {});

    fs::path dir;
    fs::path events_path;
    fs::path backup_path;
    fs::path config_path;
    fs::path attachments_dir;
    fs::path backups_dir;

    std::vector<std::string> warnings;

    std::vector<json> load_events();
    void save_events(const std::vector<json>& events);

    json load_config() const;
    void save_config(const json& config) const;

    Attachment add_attachment(const std::string& event_id, const fs::path& src);
    Attachment add_attachment_bytes(const std::string& event_id,
                                    const std::string& name, const Bytes& bytes);
    fs::path resolve(const std::string& rel) const;
    fs::path resolve(const Attachment& att) const { return resolve(att.path); }
    bool attachment_exists(const Attachment& att) const;
    void remove_attachments(const std::string& event_id) const;
    int gc_orphans(const std::vector<std::string>& keep_ids) const;

    void snapshot(const std::string& tag = "undo", int keep = 20) const;
    bool restore_last(const std::string& tag = "undo");
    bool has_snapshots(const std::string& tag = "undo") const;
    void clear_snapshots(const std::string& tag) const;

    std::optional<fs::file_time_type> events_mtime() const;
    std::string display_dir() const;
    std::string info_text() const;

private:
    void ensure_layout() const;
    std::string timestamp() const;
    std::string sanitize_name(const std::string& name) const;
    void write_backups(const std::string& text) const;
    std::vector<fs::path> backup_files(const std::string& prefix) const;
};

}  // namespace moji

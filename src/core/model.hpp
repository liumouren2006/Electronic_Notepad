// 事件与附件数据模型
#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <utility>
#include <string>
#include <vector>

#include "util.hpp"

namespace moji {

using json = nlohmann::json;

struct Attachment {
    std::string path;   // 相对数据目录，例如 attachments/<id>/a.png
    std::string name;
    std::string mime;
    long long size = 0;
    std::string added_at;

    bool is_image() const { return is_image_mime(mime); }
    bool is_audio() const { return is_audio_mime(mime); }

    json to_json() const;
    static Attachment from_json(const json& j);
};

struct Event {
    std::string id;
    std::string title;
    std::string note;
    std::optional<std::time_t> due;
    int priority = 1;
    bool done = false;
    std::optional<std::time_t> done_at;
    std::vector<std::string> tags;
    std::vector<Attachment> attachments;
    std::optional<std::time_t> created_at;
    std::optional<std::time_t> updated_at;
    int remind_before = 0;
    std::optional<std::time_t> last_reminded_at;
    std::string color;

    // 导入打包文件时的临时附件数据，不参与序列化
    std::vector<std::pair<std::string, Bytes>> pending_attachments;

    static Event create(const std::string& title,
                        const std::string& note = "",
                        std::optional<std::time_t> due = std::nullopt,
                        std::vector<std::string> tags = {},
                        int remind_before = 0,
                        const std::string& color = "");

    json to_json() const;
    static Event from_json(const json& j);

    bool is_overdue(std::optional<std::time_t> at = std::nullopt) const;
    bool is_due_on(std::time_t day) const;
    bool is_due_today() const;
    std::optional<std::time_t> remind_at() const;
    bool needs_reminder(std::time_t at) const;

    std::string display_title() const;
    std::string badge() const;
    std::string due_text() const;
};

}  // namespace moji

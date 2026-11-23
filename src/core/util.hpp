// 墨记 MoJi · C++ 版基础工具
#pragma once

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace moji {

namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

// ---------- 时间 ----------
std::time_t now_time();
std::string iso_from_time(std::time_t t);
std::string date_str(std::time_t t);        // YYYY-MM-DD
std::string clock_str(std::time_t t);       // HH:MM
std::string now_iso();
std::optional<std::time_t> parse_datetime(const std::string& text);
std::optional<std::time_t> parse_date(const std::string& text);
std::time_t start_of_day(std::time_t t);
bool same_day(std::time_t a, std::time_t b);
std::string weekday_cn(std::time_t t);
std::string humanize_due(const std::optional<std::time_t>& due,
                         std::optional<std::time_t> at = std::nullopt);

// ---------- 字符串 ----------
std::string trim(const std::string& s);
std::string lower_ascii(std::string s);
std::vector<std::string> split(const std::string& s, char delim);
std::vector<std::string> split_tags(const std::string& text);
std::string join(const std::vector<std::string>& items, const std::string& sep);
std::wstring to_wide(const std::string& s);
std::string to_utf8(const std::wstring& s);
std::string random_hex(int n);
std::string uuid8();

// ---------- 文件 ----------
fs::path home_dir();
std::string read_file(const fs::path& path, bool* ok = nullptr);
bool write_file_atomic(const fs::path& path, const std::string& text);
bool write_bytes_atomic(const fs::path& path, const Bytes& data);
bool ensure_dir(const fs::path& path);

// ---------- 附件 ----------
std::string guess_mime(const std::string& name);
bool is_image_mime(const std::string& mime);
bool is_audio_mime(const std::string& mime);
std::string human_size(long long bytes);

// ---------- 终端显示宽度 ----------
int display_width(const std::string& s);
std::string truncate_display(const std::string& s, int width);
std::string pad_display(const std::string& s, int width);
std::vector<std::string> wrap_display(const std::string& s, int width);

}  // namespace moji

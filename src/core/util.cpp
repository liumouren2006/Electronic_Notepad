#include "util.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <codecvt>
#include <locale>
#include <unistd.h>
#endif

namespace moji {

namespace {

bool parse_ymd(const std::string& text, int& y, int& m, int& d) {
    auto parts = split(text, '-');
    if (parts.size() == 3) {
        y = std::atoi(parts[0].c_str());
        m = std::atoi(parts[1].c_str());
        d = std::atoi(parts[2].c_str());
        return true;
    }
    if (parts.size() == 2) {
        std::time_t t = now_time();
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        y = tm.tm_year + 1900;
        m = std::atoi(parts[0].c_str());
        d = std::atoi(parts[1].c_str());
        return true;
    }
    return false;
}

bool parse_hms(const std::string& text, int& h, int& mi, int& s) {
    auto parts = split(text, ':');
    if (parts.empty() || parts.size() > 3) {
        return false;
    }
    h = std::atoi(parts[0].c_str());
    mi = parts.size() > 1 ? std::atoi(parts[1].c_str()) : 0;
    s = parts.size() > 2 ? std::atoi(parts[2].c_str()) : 0;
    return true;
}

}  // namespace

std::time_t now_time() { return std::time(nullptr); }

std::string iso_from_time(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    return buf;
}

std::string date_str(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return buf;
}

std::string clock_str(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M", &tm);
    return buf;
}

std::string now_iso() { return iso_from_time(now_time()); }

std::optional<std::time_t> parse_datetime(const std::string& raw) {
    std::string text = trim(raw);
    if (text.empty()) {
        return std::nullopt;
    }
    for (char& c : text) {
        if (c == '/') {
            c = '-';
        }
    }
    while (!text.empty() && (text.back() == 'Z' || text.back() == 'z')) {
        text.pop_back();
    }
    std::string date_part = text;
    std::string time_part;
    auto tpos = text.find('T');
    if (tpos != std::string::npos) {
        date_part = text.substr(0, tpos);
        time_part = text.substr(tpos + 1);
    } else {
        auto spos = text.find(' ');
        if (spos != std::string::npos) {
            date_part = text.substr(0, spos);
            time_part = text.substr(spos + 1);
        }
    }
    int y = 0, m = 0, d = 0, h = 0, mi = 0, s = 0;
    bool has_date = false;
    if (!date_part.empty()) {
        has_date = parse_ymd(date_part, y, m, d);
        if (!has_date) {
            return std::nullopt;
        }
    }
    if (!time_part.empty()) {
        if (!parse_hms(time_part, h, mi, s)) {
            return std::nullopt;
        }
    } else if (text.find(':') != std::string::npos && !has_date) {
        // 只有 HH:MM 的情况
        if (!parse_hms(text, h, mi, s)) {
            return std::nullopt;
        }
    }
    if (!has_date) {
        std::time_t now = now_time();
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &now);
#else
        localtime_r(&now, &tm);
#endif
        y = tm.tm_year + 1900;
        m = tm.tm_mon + 1;
        d = tm.tm_mday;
    }
    if (m < 1 || m > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 ||
        mi > 59 || s < 0 || s > 60 || y < 1970) {
        return std::nullopt;
    }
    std::tm tm{};
    tm.tm_year = y - 1900;
    tm.tm_mon = m - 1;
    tm.tm_mday = d;
    tm.tm_hour = h;
    tm.tm_min = mi;
    tm.tm_sec = s;
    tm.tm_isdst = -1;
    std::time_t result = std::mktime(&tm);
    if (result == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }
    return result;
}

std::optional<std::time_t> parse_date(const std::string& text) {
    return parse_datetime(text);
}

std::time_t start_of_day(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    return std::mktime(&tm);
}

bool same_day(std::time_t a, std::time_t b) { return date_str(a) == date_str(b); }

std::string weekday_cn(std::time_t t) {
    static const char* names[] = {"日", "一", "二", "三", "四", "五", "六"};
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return names[tm.tm_wday % 7];
}

std::string humanize_due(const std::optional<std::time_t>& due,
                         std::optional<std::time_t> at) {
    if (!due.has_value()) {
        return "无期限";
    }
    std::time_t now = at.value_or(now_time());
    double diff = std::difftime(*due, now);
    if (diff < 0) {
        double over = -diff;
        if (over < 3600) {
            return "已逾期 " + std::to_string(std::max<int>(1, (int)(over / 60))) + " 分钟";
        }
        if (over < 86400) {
            return "已逾期 " + std::to_string((int)(over / 3600)) + " 小时";
        }
        return "已逾期 " + std::to_string((int)(over / 86400)) + " 天";
    }
    if (diff < 3600) {
        return std::to_string(std::max<int>(1, (int)(diff / 60))) + " 分钟后";
    }
    if (same_day(*due, now)) {
        return "今天 " + clock_str(*due);
    }
    if (same_day(*due, now + 86400)) {
        return "明天 " + clock_str(*due);
    }
    if (diff < 7 * 86400) {
        return "周" + weekday_cn(*due) + " " + clock_str(*due);
    }
    std::tm tm{};
    std::time_t due_value = *due;
#ifdef _WIN32
    localtime_s(&tm, &due_value);
#else
    localtime_r(&due_value, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return buf;
}

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n\f\v";
    size_t start = s.find_first_not_of(ws);
    if (start == std::string::npos) {
        return "";
    }
    size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

std::string lower_ascii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return s;
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == delim) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

std::vector<std::string> split_tags(const std::string& text) {
    std::string normalized = text;
    const std::string cn_comma = "ï¼";  // UTF-8 的“，”
    size_t pos = 0;
    while ((pos = normalized.find(cn_comma, pos)) != std::string::npos) {
        normalized.replace(pos, cn_comma.size(), ",");
        pos += 1;
    }
    std::vector<std::string> out;
    for (auto& raw : split(normalized, ',')) {
        // 也支持用空格分隔多个标签
        std::istringstream iss(raw);
        std::string tok;
        while (iss >> tok) {
            std::string t = trim(tok);
            while (!t.empty() && t.front() == '#') {
                t.erase(t.begin());
            }
            if (!t.empty() &&
                std::find(out.begin(), out.end(), t) == out.end()) {
                out.push_back(t);
            }
        }
    }
    return out;
}

std::string join(const std::vector<std::string>& items, const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) {
            out += sep;
        }
        out += items[i];
    }
    return out;
}

std::wstring to_wide(const std::string& s) {
#ifdef _WIN32
    if (s.empty()) {
        return L"";
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (size <= 0) {
        return L"";
    }
    std::wstring out((size_t)size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], size);
    return out;
#else
    try {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
        return conv.from_bytes(s);
    } catch (...) {
        return L"";
    }
#endif
}

std::string to_utf8(const std::wstring& s) {
#ifdef _WIN32
    if (s.empty()) {
        return "";
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr,
                                   0, nullptr, nullptr);
    if (size <= 0) {
        return "";
    }
    std::string out((size_t)size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], size,
                        nullptr, nullptr);
    return out;
#else
    try {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
        return conv.to_bytes(s);
    } catch (...) {
        return "";
    }
#endif
}

std::string random_hex(int n) {
    static std::mt19937_64 rng{std::random_device{}()};
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve((size_t)n);
    for (int i = 0; i < n; ++i) {
        out.push_back(digits[rng() % 16]);
    }
    return out;
}

std::string uuid8() { return random_hex(8); }

fs::path home_dir() {
#ifdef _WIN32
    const char* profile = std::getenv("USERPROFILE");
    if (profile && *profile) {
        return fs::path(to_wide(profile));
    }
    const char* drive = std::getenv("HOMEDRIVE");
    const char* path = std::getenv("HOMEPATH");
    if (drive && path) {
        return fs::path(to_wide(std::string(drive) + path));
    }
#else
    const char* home = std::getenv("HOME");
    if (home && *home) {
        return fs::path(home);
    }
#endif
    return fs::current_path();
}

std::string read_file(const fs::path& path, bool* ok) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (ok) {
            *ok = false;
        }
        return "";
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    if (ok) {
        *ok = true;
    }
    return ss.str();
}

bool ensure_dir(const fs::path& path) {
    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec;
}

bool write_bytes_atomic(const fs::path& path, const Bytes& data) {
    if (!ensure_dir(path.parent_path())) {
        return false;
    }
    fs::path tmp = path;
    tmp += ".tmp-" + random_hex(4);
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        if (!data.empty()) {
            out.write(reinterpret_cast<const char*>(data.data()),
                      (std::streamsize)data.size());
        }
        out.flush();
        if (!out) {
            return false;
        }
    }
#ifdef _WIN32
    if (!MoveFileExW(tmp.wstring().c_str(), path.wstring().c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code ec;
        fs::remove(tmp, ec);
        return false;
    }
    return true;
#else
    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return false;
    }
    return true;
#endif
}

bool write_file_atomic(const fs::path& path, const std::string& text) {
    Bytes data(text.begin(), text.end());
    return write_bytes_atomic(path, data);
}

std::string guess_mime(const std::string& name) {
    auto pos = name.find_last_of('.');
    std::string ext = pos == std::string::npos ? "" : lower_ascii(name.substr(pos));
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".webp") return "image/webp";
    if (ext == ".bmp") return "image/bmp";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".mp3") return "audio/mpeg";
    if (ext == ".wav") return "audio/wav";
    if (ext == ".ogg") return "audio/ogg";
    if (ext == ".m4a") return "audio/mp4";
    if (ext == ".flac") return "audio/flac";
    if (ext == ".txt") return "text/plain";
    if (ext == ".md") return "text/markdown";
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".zip") return "application/zip";
    return "application/octet-stream";
}

bool is_image_mime(const std::string& mime) {
    return mime.rfind("image/", 0) == 0;
}

bool is_audio_mime(const std::string& mime) {
    return mime.rfind("audio/", 0) == 0;
}

std::string human_size(long long bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    double value = (double)bytes;
    int unit = 0;
    while (value >= 1024.0 && unit < 3) {
        value /= 1024.0;
        ++unit;
    }
    char buf[64];
    if (unit == 0) {
        std::snprintf(buf, sizeof(buf), "%lld %s", bytes, units[unit]);
    } else {
        std::snprintf(buf, sizeof(buf), "%.1f %s", value, units[unit]);
    }
    return buf;
}

namespace {
size_t next_codepoint(const std::string& s, size_t pos, uint32_t& cp) {
    if (pos >= s.size()) {
        cp = 0;
        return pos;
    }
    unsigned char c = (unsigned char)s[pos];
    if (c < 0x80) {
        cp = c;
        return pos + 1;
    }
    int extra = 0;
    if ((c & 0xE0) == 0xC0) {
        cp = c & 0x1F;
        extra = 1;
    } else if ((c & 0xF0) == 0xE0) {
        cp = c & 0x0F;
        extra = 2;
    } else if ((c & 0xF8) == 0xF0) {
        cp = c & 0x07;
        extra = 3;
    } else {
        cp = '?';
        return pos + 1;
    }
    if (pos + (size_t)extra >= s.size()) {
        cp = '?';
        return pos + 1;
    }
    for (int i = 1; i <= extra; ++i) {
        unsigned char cc = (unsigned char)s[pos + (size_t)i];
        if ((cc & 0xC0) != 0x80) {
            cp = '?';
            return pos + 1;
        }
        cp = (cp << 6) | (cc & 0x3F);
    }
    return pos + (size_t)extra + 1;
}

int cp_width(uint32_t cp) {
    if (cp == 0 || cp < 32 || (cp >= 0x7F && cp < 0xA0)) {
        return 0;
    }
    if ((cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF) ||
        (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0xFE30 && cp <= 0xFE4F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
        (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x1F300 && cp <= 0x1FAFF)) {
        return 2;
    }
    return 1;
}
}  // namespace

int display_width(const std::string& s) {
    int width = 0;
    size_t pos = 0;
    while (pos < s.size()) {
        uint32_t cp = 0;
        pos = next_codepoint(s, pos, cp);
        width += cp_width(cp);
    }
    return width;
}

std::string truncate_display(const std::string& s, int width) {
    if (display_width(s) <= width) {
        return s;
    }
    std::string out;
    int used = 0;
    size_t pos = 0;
    while (pos < s.size()) {
        size_t start = pos;
        uint32_t cp = 0;
        pos = next_codepoint(s, pos, cp);
        int w = cp_width(cp);
        if (used + w > width - 1) {
            break;
        }
        out.append(s, start, pos - start);
        used += w;
    }
    out += "…";
    return out;
}

std::string pad_display(const std::string& s, int width) {
    int w = display_width(s);
    if (w >= width) {
        return s;
    }
    return s + std::string((size_t)(width - w), ' ');
}

std::vector<std::string> wrap_display(const std::string& s, int width) {
    std::vector<std::string> lines;
    if (width <= 0) {
        return lines;
    }
    for (const auto& raw : split(s, '\n')) {
        if (raw.empty()) {
            lines.push_back("");
            continue;
        }
        std::string line;
        int used = 0;
        size_t pos = 0;
        while (pos < raw.size()) {
            size_t start = pos;
            uint32_t cp = 0;
            pos = next_codepoint(raw, pos, cp);
            int w = cp_width(cp);
            if (used + w > width) {
                lines.push_back(line);
                line.clear();
                used = 0;
            }
            line.append(raw, start, pos - start);
            used += w;
        }
        lines.push_back(line);
    }
    return lines;
}

}  // namespace moji

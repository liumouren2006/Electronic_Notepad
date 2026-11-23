#include "zip.hpp"

#include <cstring>
#include <ctime>
#include <stdexcept>

namespace moji {

namespace {
uint32_t crc_table[256];
bool crc_ready = false;

void init_crc() {
    if (crc_ready) {
        return;
    }
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        crc_table[i] = c;
    }
    crc_ready = true;
}

uint32_t crc32(const uint8_t* data, size_t len) {
    init_crc();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        c = crc_table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

void put16(Bytes& out, uint16_t v) {
    out.push_back((uint8_t)(v & 0xFF));
    out.push_back((uint8_t)((v >> 8) & 0xFF));
}
void put32(Bytes& out, uint32_t v) {
    out.push_back((uint8_t)(v & 0xFF));
    out.push_back((uint8_t)((v >> 8) & 0xFF));
    out.push_back((uint8_t)((v >> 16) & 0xFF));
    out.push_back((uint8_t)((v >> 24) & 0xFF));
}
uint16_t get16(const Bytes& data, size_t pos) {
    if (pos + 2 > data.size()) {
        throw std::runtime_error("zip 数据越界");
    }
    return (uint16_t)(data[pos] | (data[pos + 1] << 8));
}
uint32_t get32(const Bytes& data, size_t pos) {
    if (pos + 4 > data.size()) {
        throw std::runtime_error("zip 数据越界");
    }
    return (uint32_t)data[pos] | ((uint32_t)data[pos + 1] << 8) |
           ((uint32_t)data[pos + 2] << 16) | ((uint32_t)data[pos + 3] << 24);
}

void dos_time_date(uint16_t& dos_time, uint16_t& dos_date) {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    dos_time = (uint16_t)((tm.tm_hour << 11) | (tm.tm_min << 5) | (tm.tm_sec / 2));
    dos_date = (uint16_t)(((tm.tm_year + 1900 - 1980) << 9) | ((tm.tm_mon + 1) << 5) |
                          tm.tm_mday);
}
}  // namespace

Bytes zip_write(const std::vector<std::pair<std::string, Bytes>>& entries) {
    Bytes out;
    struct Central {
        std::string name;
        uint32_t crc;
        uint32_t size;
        uint32_t offset;
    };
    std::vector<Central> central;
    uint16_t dtime = 0, ddate = 0;
    dos_time_date(dtime, ddate);

    for (const auto& item : entries) {
        const std::string& name = item.first;
        const Bytes& data = item.second;
        uint32_t crc = crc32(data.empty() ? nullptr : data.data(), data.size());
        uint32_t offset = (uint32_t)out.size();
        put32(out, 0x04034b50);
        put16(out, 20);
        put16(out, 0x0800);  // UTF-8 文件名
        put16(out, 0);       // store
        put16(out, dtime);
        put16(out, ddate);
        put32(out, crc);
        put32(out, (uint32_t)data.size());
        put32(out, (uint32_t)data.size());
        put16(out, (uint16_t)name.size());
        put16(out, 0);
        out.insert(out.end(), name.begin(), name.end());
        out.insert(out.end(), data.begin(), data.end());
        central.push_back({name, crc, (uint32_t)data.size(), offset});
    }

    uint32_t cd_start = (uint32_t)out.size();
    for (const auto& c : central) {
        put32(out, 0x02014b50);
        put16(out, 20);
        put16(out, 20);
        put16(out, 0x0800);
        put16(out, 0);
        put16(out, dtime);
        put16(out, ddate);
        put32(out, c.crc);
        put32(out, c.size);
        put32(out, c.size);
        put16(out, (uint16_t)c.name.size());
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put32(out, 0);
        put32(out, c.offset);
        out.insert(out.end(), c.name.begin(), c.name.end());
    }
    uint32_t cd_size = (uint32_t)out.size() - cd_start;

    put32(out, 0x06054b50);
    put16(out, 0);
    put16(out, 0);
    put16(out, (uint16_t)central.size());
    put16(out, (uint16_t)central.size());
    put32(out, cd_size);
    put32(out, cd_start);
    put16(out, 0);
    return out;
}

std::map<std::string, Bytes> zip_read(const Bytes& data) {
    std::map<std::string, Bytes> result;
    if (data.size() < 22) {
        throw std::runtime_error("不是有效的 zip 文件");
    }
    size_t eocd = std::string::npos;
    size_t min_pos = data.size() > 65557 ? data.size() - 65557 : 0;
    for (size_t i = data.size() - 22 + 1; i-- > min_pos;) {
        if (get32(data, i) == 0x06054b50) {
            eocd = i;
            break;
        }
    }
    if (eocd == std::string::npos) {
        throw std::runtime_error("找不到 zip 目录");
    }
    uint16_t count = get16(data, eocd + 10);
    uint32_t cd_offset = get32(data, eocd + 16);
    size_t pos = cd_offset;
    for (uint16_t i = 0; i < count; ++i) {
        if (get32(data, pos) != 0x02014b50) {
            throw std::runtime_error("zip 目录损坏");
        }
        uint16_t method = get16(data, pos + 10);
        uint32_t comp_size = get32(data, pos + 20);
        uint16_t name_len = get16(data, pos + 28);
        uint16_t extra_len = get16(data, pos + 30);
        uint16_t comment_len = get16(data, pos + 32);
        uint32_t local_offset = get32(data, pos + 42);
        if (pos + 46 + name_len > data.size()) {
            throw std::runtime_error("zip 目录越界");
        }
        std::string name((const char*)&data[pos + 46], name_len);
        pos += 46 + name_len + extra_len + comment_len;
        if (!name.empty() && name.back() == '/') {
            continue;
        }
        if (method != 0) {
            throw std::runtime_error("暂不支持压缩的 zip 条目：" + name);
        }
        if (get32(data, local_offset) != 0x04034b50) {
            throw std::runtime_error("zip 条目损坏");
        }
        uint16_t lname = get16(data, local_offset + 26);
        uint16_t lextra = get16(data, local_offset + 28);
        size_t data_pos = local_offset + 30 + lname + lextra;
        if (data_pos + comp_size > data.size()) {
            throw std::runtime_error("zip 条目越界");
        }
        result[name] = Bytes(data.begin() + (std::ptrdiff_t)data_pos,
                             data.begin() + (std::ptrdiff_t)(data_pos + comp_size));
    }
    return result;
}

}  // namespace moji

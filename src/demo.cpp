#include "demo.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <vector>

namespace moji {

namespace {
std::vector<uint8_t> make_cover_bmp() {
    const int w = 320, h = 180;
    const int row = (w * 3 + 3) & ~3;
    const int data_size = row * h;
    std::vector<uint8_t> out((size_t)54 + data_size, 0);
    auto put16 = [&](int off, uint16_t v) {
        out[off] = (uint8_t)(v & 0xFF);
        out[off + 1] = (uint8_t)(v >> 8);
    };
    auto put32 = [&](int off, uint32_t v) {
        out[off] = (uint8_t)(v & 0xFF);
        out[off + 1] = (uint8_t)((v >> 8) & 0xFF);
        out[off + 2] = (uint8_t)((v >> 16) & 0xFF);
        out[off + 3] = (uint8_t)((v >> 24) & 0xFF);
    };
    out[0] = 'B';
    out[1] = 'M';
    put32(2, 54 + data_size);
    put32(10, 54);
    put32(14, 40);
    put32(18, w);
    put32(22, h);
    put16(26, 1);
    put16(28, 24);
    put32(34, data_size);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double t = (x / (double)w + y / (double)h) * 0.5;
            int r = (int)(91 + (255 - 91) * t);
            int g = (int)(141 + (126 - 141) * t);
            int b = (int)(239 + (179 - 239) * t);
            double dx = x - 270, dy = y - 42;
            if (dx * dx + dy * dy < 34 * 34) {
                r = (r + 255) / 2;
                g = (g + 255) / 2;
                b = (b + 255) / 2;
            }
            int off = 54 + (h - 1 - y) * row + x * 3;
            out[off + 0] = (uint8_t)b;
            out[off + 1] = (uint8_t)g;
            out[off + 2] = (uint8_t)r;
        }
    }
    return out;
}

std::vector<uint8_t> make_wav() {
    const int rate = 16000;
    const double seconds = 1.2;
    int frames = (int)(rate * seconds);
    std::vector<int16_t> samples;
    samples.reserve((size_t)frames);
    for (int i = 0; i < frames; ++i) {
        double t = (double)i / rate;
        double phase = std::fmod(t, 0.6);
        double env = phase < 0.45 ? std::exp(-8.0 * phase) : 0.0;
        double v = std::sin(6.28318530718 * 880.0 * t) * 0.35 * env;
        samples.push_back((int16_t)(v * 32767));
    }
    uint32_t data_size = (uint32_t)samples.size() * 2;
    std::vector<uint8_t> out;
    auto put32 = [&](uint32_t v) {
        out.push_back((uint8_t)(v & 0xFF));
        out.push_back((uint8_t)((v >> 8) & 0xFF));
        out.push_back((uint8_t)((v >> 16) & 0xFF));
        out.push_back((uint8_t)((v >> 24) & 0xFF));
    };
    auto put16 = [&](uint16_t v) {
        out.push_back((uint8_t)(v & 0xFF));
        out.push_back((uint8_t)((v >> 8) & 0xFF));
    };
    auto putstr = [&](const char* s) { out.insert(out.end(), s, s + std::strlen(s)); };
    putstr("RIFF");
    put32(36 + data_size);
    putstr("WAVEfmt ");
    put32(16);
    put16(1);
    put16(1);
    put32(rate);
    put32(rate * 2);
    put16(2);
    put16(16);
    putstr("data");
    put32(data_size);
    for (int16_t s : samples) {
        out.push_back((uint8_t)(s & 0xFF));
        out.push_back((uint8_t)((s >> 8) & 0xFF));
    }
    return out;
}
}  // namespace

int seed_demo(Repository& repo) {
    std::time_t now = now_time();
    std::time_t day = start_of_day(now);
    struct Item {
        const char* title;
        const char* note;
        int due_seconds;  // 相对今天 00:00 的秒数；INT_MIN 表示无期限
        std::vector<std::string> tags;
        int remind;
        const char* color;
    };
    const int NONE = std::numeric_limits<int>::min();
    std::vector<Item> items = {
        {"提交《电子记事本》课程设计报告",
         "## 交付清单\n- [x] 核心功能：增删改查 + 优先级顺延\n- [x] TUI 与 C++ GUI\n- [ ] 把开发日志附在报告后面\n\n> 提醒：导出 .mjnote 并拷到 U 盘",
         18 * 3600, {"课程", "截止"}, 60, "#5b8def"},
        {"复习《数据结构》链表章节",
         "重点：单链表插入/删除的指针操作，以及“插入后优先级顺延”的实现。",
         86400 + 10 * 3600, {"学习"}, 0, "#7c5cff"},
        {"跑步 3 公里", "配速 6'30\"，跑完拉伸 10 分钟。", 20 * 3600, {"运动"}, 0, "#26a69a"},
        {"给妈妈打电话", "", 2 * 86400 + 19 * 3600 + 1800, {"生活"}, 0, ""},
        {"整理实验数据并画图", "数据在 D:\\实验\\数据.csv，用 Matplotlib 画折线图。",
         -86400 - 2 * 3600, {"课程"}, 0, "#ef5350"},
        {"阅读《人月神话》第 3 章", "", NONE, {"阅读"}, 0, ""},
        {"完成服务器巡检", "", -86400 - 6 * 3600, {"运维"}, 0, ""},
        {"买牛奶", "", NONE, {"生活"}, 0, "#ffa726"},
        {"准备课程答辩 PPT",
         "结构：需求分析 → 数据结构设计 → 双界面演示 → 容错与日志。",
         4 * 86400 + 14 * 3600, {"课程", "答辩"}, 120, ""},
        {"整理桌面文件", "", NONE, {"杂事"}, 0, ""},
    };
    std::vector<std::string> ids;
    for (const auto& item : items) {
        std::optional<std::time_t> due;
        if (item.due_seconds != NONE) {
            due = day + item.due_seconds;
        }
        Event& e = repo.add(item.title, item.note, due, item.tags, std::nullopt,
                            item.remind, item.color);
        ids.push_back(e.id);
    }
    if (ids.size() > 6) {
        repo.set_done(ids[6], true);
    }
    // 生成演示附件
    fs::path tmp_cover = repo.store.dir / "_demo_cover.bmp";
    write_bytes_atomic(tmp_cover, make_cover_bmp());
    repo.add_attachment(ids[0], tmp_cover);
    std::error_code ec;
    fs::remove(tmp_cover, ec);

    fs::path tmp_wav = repo.store.dir / "_demo_ding.wav";
    Bytes wav = make_wav();
    write_bytes_atomic(tmp_wav, wav);
    repo.add_attachment(ids[2], tmp_wav);
    fs::remove(tmp_wav, ec);

    fs::path tmp_txt = repo.store.dir / "_demo_outline.txt";
    write_file_atomic(tmp_txt,
                      "一、需求分析\n二、数据结构：优先级单向链表\n三、存储：UTF-8 JSON + 附件目录\n"
                      "四、TUI / GUI 双界面\n五、容错与自动备份\n六、演示\n");
    repo.add_attachment(ids[8], tmp_txt);
    fs::remove(tmp_txt, ec);
    return (int)items.size();
}

}  // namespace moji

// GDI+ 辅助：图片加载、PNG 保存、剪贴板图片、笔迹栅格化
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace moji {

namespace fs = std::filesystem;

struct ImageData {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;  // RGBA8
};

bool imaging_init();
void imaging_shutdown();

std::optional<ImageData> load_image_rgba(const fs::path& path);
bool save_rgba_png(const fs::path& path, int width, int height,
                   const uint8_t* rgba);
bool save_clipboard_png(const fs::path& path);

struct Stroke {
    struct Point {
        float x;
        float y;
    };
    std::vector<Point> points;
    uint32_t color = 0x1F2430FF;  // RGBA
    float width = 4.0f;
};

bool save_sketch_png(const fs::path& path, int width, int height,
                     const std::vector<Stroke>& strokes);

}  // namespace moji

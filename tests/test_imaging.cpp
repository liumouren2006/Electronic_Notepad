// GDI+ 图像辅助测试（Windows）
#include <cstdio>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

#include "gui/imaging.hpp"
#include "core/util.hpp"

using namespace moji;

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    if (!imaging_init()) {
        std::cerr << "GDI+ 初始化失败\n";
        return 1;
    }
    int failures = 0;
    fs::path dir = fs::temp_directory_path() / ("moji-img-" + random_hex(6));
    fs::create_directories(dir);

    std::vector<uint8_t> rgba = {
        255, 0,   0,   255, 0,   255, 0,   255,
        0,   0,   255, 255, 255, 255, 255, 255,
    };
    fs::path png = dir / "a.png";
    if (!save_rgba_png(png, 2, 2, rgba.data())) {
        std::cerr << "save_rgba_png 失败\n";
        ++failures;
    }
    auto loaded = load_image_rgba(png);
    if (!loaded || loaded->width != 2 || loaded->height != 2) {
        std::cerr << "load_image_rgba 失败\n";
        ++failures;
    } else {
        // 左上角应为红色
        if (!(loaded->rgba[0] > 200 && loaded->rgba[1] < 60 && loaded->rgba[2] < 60)) {
            std::cerr << "像素颜色不正确\n";
            ++failures;
        }
    }

    std::vector<Stroke> strokes;
    Stroke s;
    s.color = 0x1F2430FF;
    s.width = 6;
    s.points = {{10, 10}, {100, 80}, {180, 40}};
    strokes.push_back(s);
    fs::path sketch = dir / "sketch.png";
    if (!save_sketch_png(sketch, 200, 120, strokes)) {
        std::cerr << "save_sketch_png 失败\n";
        ++failures;
    }
    if (!fs::exists(sketch) || fs::file_size(sketch) < 100) {
        std::cerr << "笔迹文件异常\n";
        ++failures;
    }

    std::error_code ec;
    fs::remove_all(dir, ec);
    imaging_shutdown();
    if (failures == 0) {
        std::cout << "ALL IMAGING TESTS PASSED\n";
        return 0;
    }
    return 1;
}

#include "gui.hpp"

#include <iostream>

namespace moji {
int run_gui(Repository&, const std::string&) {
    std::cerr << "图形界面当前仅提供 Windows 版本；Linux/macOS 请使用 moji（TUI）或命令行。\n";
    return 2;
}
}  // namespace moji

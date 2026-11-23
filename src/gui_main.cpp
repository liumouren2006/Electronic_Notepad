// 双击启动的 GUI 子系统入口（无控制台窗口）
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdlib>
#include <exception>
#include <string>

#include "core/repository.hpp"
#include "core/util.hpp"
#include "gui/gui.hpp"

using namespace moji;

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    try {
        Repository repo;
        std::string shot;
        if (const char* env = std::getenv("MOJI_SHOT")) shot = env;
        return run_gui(repo, shot);
    } catch (const std::exception& ex) {
        MessageBoxW(nullptr, to_wide(std::string("启动失败：") + ex.what()).c_str(),
                    L"墨记 MoJi", MB_ICONERROR);
        return 1;
    }
}

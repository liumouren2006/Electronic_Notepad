// 控制台版 `moji gui` 的启动器：启动同目录的 moji-gui.exe
#include "gui.hpp"

#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "core/util.hpp"

namespace moji {

int run_gui(Repository& repo, const std::string& screenshot_path) {
    wchar_t module[MAX_PATH] = {0};
    if (!GetModuleFileNameW(nullptr, module, MAX_PATH)) {
        MessageBoxW(nullptr, L"无法定位 moji.exe", L"墨记 MoJi", MB_ICONERROR);
        return 2;
    }
    fs::path exe(module);
    fs::path gui = exe.parent_path() / "moji-gui.exe";
    std::error_code ec;
    if (!fs::exists(gui, ec)) {
        MessageBoxW(nullptr,
                    (L"找不到 " + gui.wstring() +
                     L"\n请先运行 build.bat，或使用 install.ps1 安装。")
                        .c_str(),
                    L"墨记 MoJi", MB_ICONERROR);
        return 2;
    }

    std::wstring cmd = L"\"" + gui.wstring() + L"\" --data-dir \"" +
                       repo.store.dir.wstring() + L"\"";
    if (!screenshot_path.empty()) {
        cmd += L" --screenshot \"" + to_wide(screenshot_path) + L"\"";
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring mutable_cmd = cmd;
    if (!CreateProcessW(gui.wstring().c_str(), mutable_cmd.data(), nullptr, nullptr,
                        FALSE, 0, nullptr, nullptr, &si, &pi)) {
        MessageBoxW(nullptr, L"无法启动图形界面（moji-gui.exe）", L"墨记 MoJi",
                    MB_ICONERROR);
        return 2;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}

}  // namespace moji

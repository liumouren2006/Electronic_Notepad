#include "sound.hpp"

#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>

#include "util.hpp"
#endif

namespace moji {

bool play_ringtone(const std::string& path) {
    if (path.empty()) return false;
#ifdef _WIN32
    std::wstring wide = to_wide(path);
    if (PlaySoundW(wide.c_str(), nullptr,
                   SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) {
        return true;
    }
    return false;
#else
    std::cout << "\a" << std::flush;
    return false;
#endif
}

void system_beep() {
#ifdef _WIN32
    MessageBeep(MB_ICONEXCLAMATION);
#else
    std::cout << "\a" << std::flush;
#endif
}

}  // namespace moji

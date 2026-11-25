#include "sound.hpp"

#include <algorithm>
#include <cctype>
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

namespace {

std::string lower_ext(const std::string& path) {
    std::string ext;
    size_t dot = path.find_last_of('.');
    if (dot != std::string::npos) ext = path.substr(dot);
    for (char& c : ext) c = (char)std::tolower((unsigned char)c);
    return ext;
}

#ifdef _WIN32
const wchar_t* kAlias = L"moji_ringtone";
bool g_open = false;
bool g_looping = false;

void mci(const std::wstring& cmd, wchar_t* out = nullptr, size_t size = 0) {
    mciSendStringW(cmd.c_str(), out, (UINT)size, nullptr);
}

void close_open_device() {
    if (!g_open) return;
    mci(std::wstring(L"stop ") + kAlias);
    mci(std::wstring(L"close ") + kAlias);
    g_open = false;
    g_looping = false;
}

bool open_device(const std::wstring& path, const wchar_t* type) {
    close_open_device();
    std::wstring cmd = L"open \"" + path + L"\"";
    if (type && *type) {
        cmd += L" type ";
        cmd += type;
    }
    cmd += L" alias ";
    cmd += kAlias;
    if (mciSendStringW(cmd.c_str(), nullptr, 0, nullptr) == 0) {
        g_open = true;
        return true;
    }
    return false;
}
#endif

}  // namespace

bool is_supported_audio(const std::string& path) {
    const std::string ext = lower_ext(path);
    static const char* ok[] = {".wav", ".mp3",  ".wma", ".m4a", ".aac",
                               ".flac", ".ogg", ".opus", ".aiff", ".aif"};
    for (const char* e : ok) {
        if (ext == e) return true;
    }
    return false;
}

bool play_ringtone(const std::string& path, int volume_percent, bool loop) {
    if (path.empty()) return false;
    volume_percent = std::max(0, std::min(100, volume_percent));
#ifdef _WIN32
    const std::wstring wide = to_wide(path);
    if (GetFileAttributesW(wide.c_str()) == INVALID_FILE_ATTRIBUTES) return false;

    const std::string ext = lower_ext(path);
    // MCI 对不同容器需要不同的设备类型；依次尝试自动识别与常见类型。
    const wchar_t* types[3] = {nullptr, L"mpegvideo", L"waveaudio"};
    bool opened = false;
    for (const wchar_t* type : types) {
        if (open_device(wide, type)) {
            opened = true;
            break;
        }
    }

    if (opened) {
        // MCI 音量范围 0-1000。
        wchar_t vol[64];
        swprintf(vol, 64, L"setaudio %s volume to %d", kAlias,
                 (int)(volume_percent * 10));
        mci(vol);
        std::wstring play = std::wstring(L"play ") + kAlias;
        if (loop) play += L" repeat";
        if (mciSendStringW(play.c_str(), nullptr, 0, nullptr) == 0) {
            g_looping = loop;
            return true;
        }
        close_open_device();
    }

    // 回退：PlaySound 只支持 WAV；没有音量与格式控制，但保证能响。
    if (ext == ".wav") {
        DWORD flags = SND_FILENAME | SND_ASYNC | SND_NODEFAULT;
        if (loop) flags |= SND_LOOP;
        if (PlaySoundW(wide.c_str(), nullptr, flags)) {
            g_looping = loop;
            return true;
        }
    }
    return false;
#else
    std::cout << "\a" << std::flush;
    return false;
#endif
}

void stop_ringtone() {
#ifdef _WIN32
    close_open_device();
    PlaySoundW(nullptr, nullptr, 0);
#endif
}

bool is_ringtone_playing() {
#ifdef _WIN32
    if (!g_open) return false;
    wchar_t mode[32] = {0};
    wchar_t cmd[64];
    swprintf(cmd, 64, L"status %s mode", kAlias);
    mci(cmd, mode, 32);
    return wcscmp(mode, L"playing") == 0 || wcscmp(mode, L"paused") == 0;
#else
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

#pragma once

#include <string>

namespace moji {

// 播放提示音。volume_percent: 0-100；loop=true 时循环直到 stop_ringtone()。
// Windows 下优先用 MCI（支持 wav/mp3/wma/m4a 等常见格式，可调音量），
// 失败时对 wav 回退 PlaySound，再失败返回 false。
bool play_ringtone(const std::string& path, int volume_percent = 100, bool loop = false);

// 停止当前铃声。
void stop_ringtone();

// 是否正在播放铃声。
bool is_ringtone_playing();

// 扩展名是否是支持的音频（用于选择文件过滤器与提示）。
bool is_supported_audio(const std::string& path);

// 系统蜂鸣（跨平台兜底）
void system_beep();

}  // namespace moji

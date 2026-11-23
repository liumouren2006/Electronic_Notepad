#pragma once

#include <string>

namespace moji {

// 播放自定义铃声（Windows 支持 WAV；失败返回 false）
bool play_ringtone(const std::string& path);

// 系统蜂鸣（跨平台兜底）
void system_beep();

}  // namespace moji

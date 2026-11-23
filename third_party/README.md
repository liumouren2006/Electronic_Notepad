# 第三方库

本目录随仓库提供以下依赖，构建时无需联网：

| 库 | 版本 | 用途 | 许可证 |
| --- | --- | --- | --- |
| [Dear ImGui](https://github.com/ocornut/imgui) | v1.91.0 | GUI（Win32 + DX11 后端） | MIT，见 `LICENSE-imgui.txt` |
| [nlohmann/json](https://github.com/nlohmann/json) | v3.11.3 | JSON 读写（单头文件） | MIT，见 `LICENSE-nlohmann.txt` |

ImGui 只保留了核心源码与 `backends/imgui_impl_win32.*`、`backends/imgui_impl_dx11.*`，示例/文档/其它后端已移除以减小仓库体积。

构建命令见根目录 `build.bat` / `build.sh`。

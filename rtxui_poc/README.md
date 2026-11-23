# 墨记 MoJi · RTXUI TUI PoC

> 独立分支：`feature/rtxui-tui`  
> 目的：验证用 **RTXUI**（C++23、HTML/CSS 驱动的终端 UI）重写 Python/Textual 版 TUI 的可行性。
>
> **不会替换当前可用的 Win32 TUI**；`src/tui/` 和 `build.bat` 不受影响。

## 为什么是 RTXUI

RTXUI 是 FTXUI 作者的新项目，用 HTML 写结构、CSS 写布局，支持 flex/grid、`:focus`、`<dialog>`、`<markdown>`、表格、滚动容器、鼠标和过渡动画，语义上最接近 Textual。它不是 Textual 的移植，但比 FTXUI 更接近 Textual 的“用样式描述界面”的方式。

- 上游：<https://github.com/ArthurSonzogni/RTXUI>
- 许可证：MIT

## 已实现

- 真实读取 `events.json`，复用 `src/core` 的 `Repository` / `Event`；
- 顶栏、侧栏统计、搜索框、视图筛选、两行事项卡片、详情 Markdown、附件列表、底栏；
- 鼠标点击卡片选择；`↑↓/j k` 选择；
- `/` 聚焦搜索，实时过滤标题/正文/标签/附件名；
- `a` 新建、`e`/Enter 编辑：居中 `<dialog>`，字段含标题、截止、提醒、优先级位置、标签、备注、Windows 通知；
- `F1–F4` 快捷时间（今天/明天/下周一/清除）；
- `Space`/`d` 完成，`x`/Delete 删除确认，`[`/`]` 调序；
- `o` 附件弹窗：打开/移除；
- `?` 帮助弹窗（Markdown 表格）；
- `Ctrl+Z` / `Ctrl+Y` 撤销/重做；`q` 退出；
- 终端尺寸变化由 flex 布局自动适应。

## 构建

要求：Windows + MSYS2 UCRT64、GCC 15（支持 C++23）、CMake ≥ 3.24、Ninja。

```powershell
cd D:\程序设计\moji-note
python -m pip install cmake ninja
.\rtxui_poc\fetch_rtxui.ps1
# GitHub 直连失败时，先给 git 配代理，例如：
#   git config --global http.proxy http://127.0.0.1:7890

$env:PATH = "$PWD\.venv\Scripts;C:\msys64\ucrt64\bin;$env:PATH"
cmake -S rtxui_poc -B rtxui_poc\build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build rtxui_poc\build --target moji-rtxui -j4
```

> 注意：不要用 `MinGW Makefiles` 生成器从含中文的路径构建；`mingw32-make` 的 `CreateProcess` 会把中文路径弄坏。Ninja 可以正常处理中文路径。

如果 RTXUI 已经克隆到别处：

```powershell
cmake -S rtxui_poc -B rtxui_poc\build -G Ninja `
  -DRTXUI_DIR=C:\path\to\RTXUI
```

## 运行

```powershell
rtxui_poc\build\moji-rtxui.exe --data-dir D:\some\moji-data
```

无终端环境下可以跑 headless：

```powershell
$env:RTXUI_HEADLESS = "120x40"
Get-Content NUL | rtxui_poc\build\moji-rtxui.exe --data-dir D:\some\moji-data
```

输出为纯文本屏幕，便于自动检查布局和交互。

## 已知差距

- 主题固定为 Python 版配色；暂未读取 `tui-theme.json`；
- 导入/导出、铃声、系统通知、标签点击筛选尚未接入；
- 与 Textual 不是同一渲染器，行高、滚动条、动画细节仍会有差异；
- 目前是 PoC，不参与 `build.bat` / `package.ps1` 的正式打包。

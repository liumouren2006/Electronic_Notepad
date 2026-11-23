# C/C++ 有没有 Textual 的替代品？

> 目的：评估能否用现成的 C/C++ TUI 框架“完全复刻”Python 原型里的 Textual TUI。
> 结论：**没有 Textual 的 C++ 移植；目前最接近的是 RTXUI（HTML/CSS + C++23），其次是 FTXUI。**

## 1. 候选项目

| 项目 | 语言/许可 | 模型 | 和 Textual 的差距 |
| --- | --- | --- | --- |
| [RTXUI](https://github.com/ArthurSonzogni/RTXUI) | C++23，MIT | **HTML/CSS + 响应式绑定**，flex/grid、`:focus`、`<dialog>`、`<markdown>`、滚动容器 | **目前最接近 Textual 的 C++ 项目**：布局/CSS/焦点/动画/鼠标语义都很像；但需要 CMake + C++23，API 与 Textual 不同，不能直接“套壳” |
| [FTXUI](https://github.com/ArthurSonzogni/FTXUI) | C++17/20，MIT | 函数式/即时模式，flexbox 布局，组件树 | 最接近的 C++ TUI；有输入框、菜单、滚动、模态、颜色，但没有 Textual 的 CSS、`ModalScreen`、Rich Markdown、动画/焦点系统 |
| [TermOx](https://github.com/a-n-t-h-o-n-y/TermOx) | C++20 | 信号/槽、widget 树、定时器 | 比 FTXUI 更“组件化”，但仍不是 Textual 的 CSS/retained 模型；需要确认本机 Windows/ConPTY 构建链 |
| [ImTui](https://github.com/ggerganov/imtui) | C++ | 把 Dear ImGui 渲染到终端 | 即时模式 GUI 风格，和 Textual 的布局/交互差异更大 |
| [Turbo Vision 2.0 port](https://github.com/magiblot/tvision) | C++14 | 经典 DOS 窗口/菜单/对话框 | 适合传统窗口程序，不是 Textual 风格 |
| [Notcurses](https://github.com/dankamongmen/notcurses) | C，Apache-2.0 | 低层终端合成器、plane、真彩色/图像 | 能力强但偏底层，需要自己做布局/控件/Markdown |
| ncurses | C | 字符终端库 | 更底层，不是高层组件框架 |

## 2. 为什么不能“直接换库就 1:1”

Python 版 TUI 的视觉不是单纯“画字符”，而是 Textual 提供的：

- `ModalScreen` 的焦点/遮罩/按钮；
- `ListView` 的滚动条、高亮、悬停；
- `Input` / `TextArea` 的编辑光标、选区；
- `Rich Markdown` 的标题/表格/行内样式；
- CSS 布局、Footer 动态提示、命令面板、动画。

FTXUI/TermOx 能实现“类似控件”，但没有同一套 CSS/Textual 组件语义。换库意味着重写整个 TUI，而且最终仍是“像 Textual”，不会逐像素相同；不同终端字体、行高、宽字符处理也会造成差异。

## 3. 本次采用的做法

不替换现有 Win32 Console + ANSI 渲染器，而是在其上补齐 Textual 版最明显的交互：

- `a` / `e` 改为 Raw 模式下的居中模态表单：标题、截止、提醒、位置、标签、多行备注、F1–F4 快捷时间、取消/保存按钮；
- `?` 居中帮助面板；
- `/` 内联边框搜索框；
- `o` 附件弹窗；
- Markdown 表格框线渲染；
- 去掉侧栏底部重复的灰色快捷键提示，只保留全局 Footer。

这样既保留当前工程“纯 C++、无新依赖、Windows 可直接构建”的优点，又把与 Python 版差异最大的编辑表单补上。

## 4. RTXUI PoC（本分支已实现）

已在独立分支 `feature/rtxui-tui` 中把 RTXUI 接进来，代码在 `rtxui_poc/`，复用 `src/core`，不改现有 `src/tui`。CMake + Ninja 构建，Windows/MinGW GCC 15.2 通过；可执行文件静态链接，干净 PATH 下可运行。

构建（中文路径必须用 Ninja，`MinGW Makefiles` 的 `mingw32-make` 会因 CreateProcess 乱码失败）：

```powershell
python -m pip install cmake ninja
.\rtxui_poc\fetch_rtxui.ps1
$env:PATH = "$PWD\.venv\Scripts;C:\msys64\ucrt64\bin;$env:PATH"
cmake -S rtxui_poc -B rtxui_poc\build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build rtxui_poc\build --target moji-rtxui -j4
```

已实现：三栏布局、搜索、筛选、Markdown 详情、新增/编辑 dialog、完成/删除确认、附件弹窗、帮助、撤销/重做、鼠标/Tab。详细见 `rtxui_poc/README.md`。

RTXUI 是更值得尝试的路线：

1. 安装 CMake ≥3.24，用 FetchContent 或把 RTXUI 源码放进 `third_party/rtxui`；
2. 新建 `src/tui_rtxui/`，保留 `Repository`，把界面写成 HTML/CSS：
   - 左侧栏 + 列表 + 详情三栏用 flex；
   - 卡片列表用 `<for each="{items}" as="item">`，选中行加 `selected` class；
   - 搜索用 `<input value="{search}">`，在 `Digest()` 里过滤；
   - 新建/编辑用 `<dialog open="{editing}">` + `<input>`/`<textarea>`；
   - 帮助/附件用 `<dialog>`，详情备注用 `<markdown content="{note}">`；
3. 保留现有 Win32 TUI 作为 fallback，先并排跑，确认中文、鼠标、ConPTY、滚动无误再切换；
4. 主题可以继续读 `tui-theme.json`，动态拼 CSS 变量。

风险：RTXUI 是 C++23 新库，构建链从 g++ 单命令变成 CMake；旧 TUI 的坐标/键盘/ConPTY 测试全部要重写；最终是“用 HTML/CSS 写一个类似 Textual 的界面”，不是直接复用 Textual 的颜色/布局。

## 5. 最终取舍

- 正式 `master` 仍保留 Win32 Console + ANSI TUI，保证 `build.bat` / `package.ps1` 无新依赖、可直接打包；
- RTXUI PoC 放在独立分支 `feature/rtxui-tui`，构建产物在 `D:\程序设计\moji-rtxui-poc\`，不参与正式包；
- 在 RTXUI 分支上验证了“C++ 也能用 HTML/CSS 写出接近 Textual 的三栏 + dialog + Markdown 界面”；
- 后续若要把 RTXUI 变成正式 TUI，需要补齐主题 JSON、导入导出、提醒、标签筛选，并决定是否接受 C++23/CMake 依赖。

## 参考

- [RTXUI GitHub](https://github.com/ArthurSonzogni/RTXUI)
- [FTXUI GitHub](https://github.com/ArthurSonzogni/FTXUI)
- [TermOx GitHub](https://github.com/a-n-t-h-o-n-y/TermOx)
- [FTXUI vs Notcurses (LibHunt)](https://www.libhunt.com/compare-notcurses-vs-FTXUI)
- [TUI library comparison (sparkles)](https://github.com/PetarKirov/sparkles/blob/main/docs/research/tui-libraries/comparison.md)
- [TUI framework comparison (melker)](https://github.com/wistrand/melker/blob/main/agent_docs/tui-comparison.md)

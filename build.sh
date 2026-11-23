#!/usr/bin/env sh
# 构建脚本：Windows(MSYS2/MinGW) 构建完整 GUI；其它平台构建核心 + TUI/CLI
set -e
cd "$(dirname "$0")"
CXX="${CXX:-g++}"
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -DUNICODE -D_UNICODE -Isrc -Ithird_party -Ithird_party/imgui -Ithird_party/imgui/backends"
CORE="src/core/util.cpp src/core/model.cpp src/core/elist.cpp src/core/store.cpp src/core/zip.cpp src/core/repository.cpp src/core/importer.cpp"
IMGUI="third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp third_party/imgui/backends/imgui_impl_win32.cpp third_party/imgui/backends/imgui_impl_dx11.cpp"
mkdir -p build

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        LIBS="-static -static-libgcc -static-libstdc++ -ld3d11 -ldxgi -ld3dcompiler -ldwmapi -lgdiplus -lgdi32 -luser32 -lshell32 -lcomdlg32 -lole32 -luuid -limm32 -lcomctl32"
        echo "=== 编译 moji.exe ==="
        # shellcheck disable=SC2086
        "$CXX" $CXXFLAGS src/main.cpp src/demo.cpp src/tui/tui.cpp src/gui/gui.cpp src/gui/imaging.cpp $CORE $IMGUI -o build/moji.exe $LIBS
        echo "=== 编译 moji-gui.exe ==="
        # shellcheck disable=SC2086
        "$CXX" $CXXFLAGS -mwindows -municode src/gui_main.cpp src/gui/gui.cpp src/gui/imaging.cpp $CORE $IMGUI -o build/moji-gui.exe $LIBS
        ;;
    *)
        echo "=== 非 Windows：编译核心 + TUI/CLI（不含 ImGui GUI） ==="
        # shellcheck disable=SC2086
        "$CXX" $CXXFLAGS src/main.cpp src/demo.cpp src/tui/tui.cpp src/gui/gui_posix.cpp $CORE -o build/moji
        ;;
esac

echo "=== 编译核心测试 ==="
# shellcheck disable=SC2086
"$CXX" $CXXFLAGS tests/test_core.cpp $CORE -o build/test_core
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        echo "=== 编译图像测试 ==="
        # shellcheck disable=SC2086
        "$CXX" $CXXFLAGS tests/test_imaging.cpp src/gui/imaging.cpp src/core/util.cpp -o build/test_imaging.exe $LIBS
        ;;
esac
echo "完成，产物在 build/"

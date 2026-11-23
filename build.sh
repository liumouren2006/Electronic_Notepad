#!/usr/bin/env sh
# 构建脚本：Windows(MSYS2) 构建 Qt GUI；其它平台构建核心 + 菜单式 TUI/CLI
set -e
cd "$(dirname "$0")"
CXX="${CXX:-g++}"
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -DUNICODE -D_UNICODE -Isrc -Ithird_party"
CORE="src/core/util.cpp src/core/model.cpp src/core/elist.cpp src/core/store.cpp src/core/zip.cpp src/core/repository.cpp src/core/importer.cpp"
mkdir -p build

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        echo "=== 编译 moji.exe（CLI / TUI / GUI 启动器） ==="
        # shellcheck disable=SC2086
        "$CXX" $CXXFLAGS src/main.cpp src/demo.cpp src/tui/tui.cpp src/gui/launcher.cpp $CORE -o build/moji.exe -static -static-libgcc -static-libstdc++ -luser32 -lshell32

        echo "=== 编译 Qt GUI moji-gui.exe ==="
        mkdir -p build/qt-build
        (cd build/qt-build && qmake6 ../../moji_gui.pro && mingw32-make -j4)

        echo "=== 部署 Qt 运行库 ==="
        windeployqt --release --no-translations --no-opengl-sw --no-system-d3d-compiler --dir build build/moji-gui.exe
        echo "=== 复制 MinGW / ICU 运行库 ==="
        for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll \
                   libb2-1.dll libbrotlicommon.dll libbrotlidec.dll libbz2-1.dll \
                   libdouble-conversion.dll libfreetype-6.dll libglib-2.0-0.dll \
                   libgraphite2.dll libharfbuzz-0.dll libiconv-2.dll libintl-8.dll \
                   libjpeg-8.dll libmd4c.dll libpcre2-16-0.dll libpcre2-8-0.dll libpng16-16.dll \
                   libzstd.dll zlib1.dll; do
            [ -f "/c/msys64/ucrt64/bin/$dll" ] && cp -f "/c/msys64/ucrt64/bin/$dll" build/
        done
        for dll in /c/msys64/ucrt64/bin/libicudt*.dll /c/msys64/ucrt64/bin/libicuin*.dll /c/msys64/ucrt64/bin/libicuuc*.dll; do
            [ -f "$dll" ] && cp -f "$dll" build/
        done
        ;;
    *)
        echo "=== 非 Windows：编译核心 + 菜单式 TUI/CLI（不含 Qt GUI） ==="
        # shellcheck disable=SC2086
        "$CXX" $CXXFLAGS src/main.cpp src/demo.cpp src/tui/tui.cpp src/gui/gui_posix.cpp $CORE -o build/moji
        ;;
esac

echo "=== 编译核心测试 ==="
# shellcheck disable=SC2086
"$CXX" $CXXFLAGS tests/test_core.cpp $CORE -o build/test_core
echo "完成，产物在 build/"

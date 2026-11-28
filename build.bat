@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

REM Prefer MSYS2 UCRT64 toolchain (g++, qmake6, mingw32-make, windeployqt)
if exist "C:\msys64\ucrt64\bin\g++.exe" set "PATH=C:\msys64\ucrt64\bin;%PATH%"
where g++ >nul 2>nul
if errorlevel 1 (
    echo [ERROR] g++ not found. Install MSYS2 and add C:\msys64\ucrt64\bin to PATH.
    exit /b 1
)
where qmake6 >nul 2>nul
if errorlevel 1 (
    echo [ERROR] qmake6 not found. Install Qt6: pacman -S mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-tools
    exit /b 1
)
if not exist build mkdir build

set "CXXFLAGS=-std=c++17 -O2 -Wall -Wextra -DUNICODE -D_UNICODE -Isrc -Ithird_party"
set "CORE=src\core\util.cpp src\core\model.cpp src\core\elist.cpp src\core\store.cpp src\core\zip.cpp src\core\repository.cpp src\core\importer.cpp src\core\sound.cpp"

echo === Building console moji.exe (CLI / TUI / GUI launcher) ===
windres -I srcssets -i srcpp.rc -o buildpp_res.o
if errorlevel 1 exit /b 1
g++ %CXXFLAGS% src\main.cpp src\demo.cpp src\tui\tui.cpp src\gui\launcher.cpp %CORE% buildpp_res.o -o build\moji.exe -static -static-libgcc -static-libstdc++ -luser32 -lshell32 -lwinmm
if errorlevel 1 exit /b 1

echo === Building Qt GUI moji-gui.exe ===
if not exist build\qt-build mkdir build\qt-build
copy /Y srcssetspp.ico build\qt-buildpp.ico >nul
copy /B srcssetsssets.qrc+,, srcssetsssets.qrc >nul
pushd build\qt-build
qmake6 ..\..\moji_gui.pro
if errorlevel 1 (
    popd
    exit /b 1
)
mingw32-make -j4
if errorlevel 1 (
    popd
    exit /b 1
)
popd
if not exist build\moji-gui.exe (
    echo [ERROR] Qt build did not produce build\moji-gui.exe
    exit /b 1
)

echo === Deploying Qt runtime (windeployqt) ===
windeployqt --release --no-translations --no-opengl-sw --no-system-d3d-compiler --dir build build\moji-gui.exe
if errorlevel 1 exit /b 1

echo === Copying MinGW/Qt runtime DLLs ===
for %%F in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll libb2-1.dll libbrotlicommon.dll libbrotlidec.dll libbz2-1.dll libdouble-conversion.dll libfreetype-6.dll libglib-2.0-0.dll libgraphite2.dll libharfbuzz-0.dll libiconv-2.dll libintl-8.dll libjpeg-8.dll libmd4c.dll libpcre2-16-0.dll libpcre2-8-0.dll libpng16-16.dll libzstd.dll zlib1.dll) do (
    if exist "C:\msys64\ucrt64\bin\%%F" copy /Y "C:\msys64\ucrt64\bin\%%F" build >nul
)
for %%F in ("C:\msys64\ucrt64\bin\libicudt*.dll" "C:\msys64\ucrt64\bin\libicuin*.dll" "C:\msys64\ucrt64\bin\libicuuc*.dll") do (
    if exist "%%~fF" copy /Y "%%~fF" build >nul
)

echo === Building core tests ===
g++ %CXXFLAGS% tests\test_core.cpp %CORE% -o build\test_core.exe -static -static-libgcc -static-libstdc++ -lwinmm
if errorlevel 1 exit /b 1

echo.
echo Done:
echo   build\moji.exe       console / TUI / CLI / "moji gui" launcher
echo   build\moji-gui.exe   Qt Widgets GUI (with Qt runtime beside it)
echo   build\test_core.exe  core tests (79 checks)

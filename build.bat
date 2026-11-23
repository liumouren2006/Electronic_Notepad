@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

REM Prefer MSYS2 UCRT64 g++ (cc1 needs its DLLs on PATH)
if exist "C:\msys64\ucrt64\bin\g++.exe" set "PATH=C:\msys64\ucrt64\bin;%PATH%"
where g++ >nul 2>nul
if errorlevel 1 (
    echo [ERROR] g++ not found. Install MSYS2 and add C:\msys64\ucrt64\bin to PATH.
    exit /b 1
)
if not exist build mkdir build

set "CXXFLAGS=-std=c++17 -O2 -Wall -Wextra -DUNICODE -D_UNICODE -Isrc -Ithird_party -Ithird_party/imgui -Ithird_party/imgui/backends"
set "LIBS=-static -static-libgcc -static-libstdc++ -ld3d11 -ldxgi -ld3dcompiler -ldwmapi -lgdiplus -lgdi32 -luser32 -lshell32 -lcomdlg32 -lole32 -luuid -limm32 -lcomctl32"
set "CORE=src\core\util.cpp src\core\model.cpp src\core\elist.cpp src\core\store.cpp src\core\zip.cpp src\core\repository.cpp src\core\importer.cpp"
set "IMGUI=third_party\imgui\imgui.cpp third_party\imgui\imgui_draw.cpp third_party\imgui\imgui_tables.cpp third_party\imgui\imgui_widgets.cpp third_party\imgui\backends\imgui_impl_win32.cpp third_party\imgui\backends\imgui_impl_dx11.cpp"

echo === Building console moji.exe (TUI / CLI / GUI) ===
g++ %CXXFLAGS% src\main.cpp src\demo.cpp src\tui\tui.cpp src\gui\gui.cpp src\gui\imaging.cpp %CORE% %IMGUI% -o build\moji.exe %LIBS%
if errorlevel 1 exit /b 1

echo === Building windowed moji-gui.exe ===
g++ %CXXFLAGS% -mwindows -municode src\gui_main.cpp src\gui\gui.cpp src\gui\imaging.cpp %CORE% %IMGUI% -o build\moji-gui.exe %LIBS%
if errorlevel 1 exit /b 1

echo === Building core tests ===
g++ %CXXFLAGS% tests\test_core.cpp %CORE% -o build\test_core.exe -static -static-libgcc -static-libstdc++
if errorlevel 1 exit /b 1

echo === Building imaging tests ===
g++ %CXXFLAGS% tests\test_imaging.cpp src\gui\imaging.cpp src\core\util.cpp -o build\test_imaging.exe -static -static-libgcc -static-libstdc++ -lgdiplus -lgdi32 -lole32 -luuid
if errorlevel 1 exit /b 1

echo.
echo Done:
echo   build\moji.exe          console / TUI / GUI
echo   build\moji-gui.exe      windowed GUI
echo   build\test_core.exe     core tests (62 checks)
echo   build\test_imaging.exe  GDI+ image tests

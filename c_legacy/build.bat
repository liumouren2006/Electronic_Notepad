@echo off
REM 编译 C 版（MinGW gcc）。若使用 VC6，请在 IDE 中新建控制台工程并加入 notebook.c。
cd /d "%~dp0"
gcc -std=c89 -Wall -Wextra -O2 -o notebook.exe notebook.c
if errorlevel 1 (
    echo 编译失败：请确认 gcc 在 PATH 中（MSYS2 的 ucrt64\bin 也要加入 PATH）。
    exit /b 1
)
echo 已生成 notebook.exe

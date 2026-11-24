@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
echo 墨记 MoJi 安装程序
echo.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
    echo 安装未完成，退出码 %RC%。
    pause
    exit /b %RC%
)
echo 安装成功。
pause
exit /b 0

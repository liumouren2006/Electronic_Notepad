@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
echo 墨记 MoJi 卸载程序
echo.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0uninstall.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
pause
exit /b %RC%

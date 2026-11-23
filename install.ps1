# 墨记 MoJi（C++ 版）安装脚本
# 用法：powershell -ExecutionPolicy Bypass -File .\install.ps1
#       .\install.ps1 -NoBuild      # 使用已编译的 build\ 产物
#       .\install.ps1 -Desktop      # 额外创建桌面快捷方式
param(
    [switch]$NoBuild,
    [switch]$Desktop
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

if (-not $NoBuild) {
    Write-Host "正在编译（需要 MSYS2 UCRT64 的 g++）..."
    & cmd /c "`"$root\build.bat`""
    if ($LASTEXITCODE -ne 0) {
        throw "编译失败。请确认已安装 MSYS2 且 C:\msys64\ucrt64\bin 在 PATH 中。"
    }
}

$build = Join-Path $root "build"
$dest = Join-Path $env:LOCALAPPDATA "MoJi\bin"
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Copy-Item (Join-Path $build "moji.exe") $dest -Force
Copy-Item (Join-Path $build "moji-gui.exe") $dest -Force
# Qt 运行库（windeployqt 产物）与 MinGW/ICU 依赖
Get-ChildItem $build -Filter "*.dll" -File | Copy-Item -Destination $dest -Force
foreach ($dir in @("platforms","styles","imageformats","iconengines","tls","networkinformation","generic","platforminputcontexts")) {
    $src = Join-Path $build $dir
    if (Test-Path $src) {
        Copy-Item $src (Join-Path $dest $dir) -Recurse -Force
    }
}
if (-not (Test-Path (Join-Path $dest "Qt6Core.dll"))) {
    Write-Warning "未找到 Qt6 运行库；moji-gui.exe 可能无法启动。请重新运行 build.bat。"
}

$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
$already = $false
if ($userPath) {
    foreach ($part in ($userPath -split ';')) {
        if ($part -and ($part.TrimEnd('\') -ieq $dest.TrimEnd('\'))) { $already = $true; break }
    }
}
if (-not $already) {
    $newPath = if ([string]::IsNullOrWhiteSpace($userPath)) { $dest } else { $userPath.TrimEnd(';') + ";" + $dest }
    [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
    Write-Host "已把 $dest 写入用户 PATH，重新打开终端后可用 moji 命令。"
}
$env:Path = $env:Path + ";" + $dest

$shell = New-Object -ComObject WScript.Shell
$startMenu = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs"
$lnk = $shell.CreateShortcut((Join-Path $startMenu "墨记 MoJi.lnk"))
$lnk.TargetPath = Join-Path $dest "moji-gui.exe"
$lnk.WorkingDirectory = $dest
$lnk.Description = "墨记 MoJi 电子记事本"
$lnk.Save()
if ($Desktop) {
    $desktopLnk = $shell.CreateShortcut((Join-Path ([Environment]::GetFolderPath("Desktop")) "墨记 MoJi.lnk"))
    $desktopLnk.TargetPath = Join-Path $dest "moji-gui.exe"
    $desktopLnk.WorkingDirectory = $dest
    $desktopLnk.Save()
}

Write-Host ""
Write-Host "安装完成。"
Write-Host "  终端：moji            进入 TUI"
Write-Host "  终端：moji gui        进入 GUI"
Write-Host "  或双击开始菜单里的「墨记 MoJi」"
Write-Host "数据目录：$HOME\.moji（可用 MOJI_HOME 或 --data-dir 修改）"

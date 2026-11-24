# 墨记 MoJi（C++ 版）安装脚本
# 用法：
#   install.bat                     交互式：检测环境，缺依赖时询问是否自动安装
#   install.bat -Yes                不询问，能装就装、不能装就报错
#   install.bat -NoBuild            使用已编译的 build\ 产物
#   install.bat -Desktop            额外创建桌面快捷方式
#   install.bat -InstallDeps        允许通过 winget + pacman 自动安装 MSYS2/Qt6
param(
    [switch]$NoBuild,
    [switch]$Desktop,
    [switch]$InstallDeps,
    [switch]$Yes,
    [switch]$NoPath,
    [switch]$NoShortcut
)
try { [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new() } catch { }

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
function Write-Step([string]$text) { Write-Host "==> $text" -ForegroundColor Cyan }
function Write-Ok([string]$text) { Write-Host "    $text" -ForegroundColor Green }
function Write-Warn2([string]$text) { Write-Host "    $text" -ForegroundColor Yellow }
function Add-Msys2ToPath {
    $bin = "C:\msys64\ucrt64\bin"
    if (Test-Path $bin) {
        $env:Path = "$bin;$env:Path"
        return $true
    }
    return $false
}
function Get-Tool([string]$name) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}
function Test-BuildReady {
    $gxx = Get-Tool "g++.exe"
    $qmake = Get-Tool "qmake6.exe"
    $make = Get-Tool "mingw32-make.exe"
    $deploy = Get-Tool "windeployqt.exe"
    return [pscustomobject]@{
        Gxx = $gxx; Qmake = $qmake; Make = $make; Deploy = $deploy
        Ready = [bool]($gxx -and $qmake -and $make -and $deploy)
    }
}
function Test-BuildOutput {
    $build = Join-Path $root "build"
    $need = @("moji.exe", "moji-gui.exe", "Qt6Core.dll", "platforms\qwindows.dll")
    $missing = @()
    foreach ($item in $need) {
        if (-not (Test-Path (Join-Path $build $item))) { $missing += $item }
    }
    return [pscustomobject]@{ Ok = ($missing.Count -eq 0); Missing = $missing }
}
function Install-BuildDeps {
    Write-Step "自动安装构建依赖（MSYS2 + GCC + Qt6）"
    $winget = Get-Tool "winget.exe"
    if (-not $winget) {
        throw "未找到 winget。请手动安装 MSYS2，然后在 MSYS2 中执行：pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-tools mingw-w64-ucrt-x86_64-make"
    }
    if (-not (Test-Path "C:\msys64\usr\bin\bash.exe")) {
        Write-Host "    通过 winget 安装 MSYS2（可能需要管理员确认）..."
        & $winget install --id MSYS2.MSYS2 -e --silent --accept-package-agreements --accept-source-agreements --disable-interactivity
        if ($LASTEXITCODE -ne 0 -and -not (Test-Path "C:\msys64\usr\bin\bash.exe")) {
            throw "MSYS2 安装失败（winget 退出码 $LASTEXITCODE）。请手动安装后重试。"
        }
    } else {
        Write-Ok "已存在 C:\msys64"
    }
    $bash = "C:\msys64\usr\bin\bash.exe"
    if (-not (Test-Path $bash)) { throw "找不到 $bash" }
    Write-Host "    更新 pacman 数据库..."
    & $bash -lc "pacman -Sy --noconfirm --needed"
    Write-Host "    安装 GCC / Qt6 / make（约 1 GB，首次较慢）..."
    & $bash -lc "pacman -S --noconfirm --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-tools mingw-w64-ucrt-x86_64-make"
    if ($LASTEXITCODE -ne 0) {
        throw "pacman 安装依赖失败。请在 MSYS2 UCRT64 终端手动执行上面的 pacman 命令。"
    }
    Add-Msys2ToPath | Out-Null
}
function Confirm-Action([string]$question, [bool]$defaultYes) {
    if ($Yes) { return $true }
    $suffix = if ($defaultYes) { "[Y/n]" } else { "[y/N]" }
    $answer = Read-Host "$question $suffix"
    if ([string]::IsNullOrWhiteSpace($answer)) { return $defaultYes }
    return ($answer -match '^(y|yes|是)$')
}
# ---------- 0. 基本环境 ----------
Write-Step "检查系统环境"
if (-not [Environment]::Is64BitOperatingSystem) {
    throw "墨记 MoJi 需要 64 位 Windows。"
}
$ver = [Environment]::OSVersion.Version
if ($ver.Major -lt 10) {
    Write-Warn2 "检测到 Windows $($ver.Major).$($ver.Minor)，建议 Windows 10/11。"
} else {
    Write-Ok "Windows $($ver.Major).$($ver.Minor) 64 位"
}
if (-not (Test-Path $env:LOCALAPPDATA)) {
    throw "找不到 LOCALAPPDATA，无法进行用户级安装。"
}
# ---------- 1. 是否已有可运行产物 ----------
$output = Test-BuildOutput
if ($output.Ok -and -not $NoBuild) {
    Write-Ok "检测到已编译的 build\ 产物，跳过编译"
    $NoBuild = $true
}
# ---------- 2. 构建环境 ----------
if (-not $NoBuild) {
    Add-Msys2ToPath | Out-Null
    $tools = Test-BuildReady
    if (-not $tools.Ready) {
        Write-Warn2 "构建环境不完整："
        if (-not $tools.Gxx) { Write-Warn2 "  缺少 g++" }
        if (-not $tools.Qmake) { Write-Warn2 "  缺少 qmake6" }
        if (-not $tools.Make) { Write-Warn2 "  缺少 mingw32-make" }
        if (-not $tools.Deploy) { Write-Warn2 "  缺少 windeployqt" }
        $canInstall = [bool]$InstallDeps
        if (-not $canInstall) {
            $canInstall = Confirm-Action "是否自动安装 MSYS2 + GCC + Qt6？（约 1 GB）" $false
        }
        if (-not $canInstall) {
            throw @"
构建环境不完整，且未选择自动安装。
请任选其一：
  1. 运行 install.bat -InstallDeps 自动安装；
  2. 手动安装 MSYS2 后，在 MSYS2 UCRT64 终端执行：
       pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-tools mingw-w64-ucrt-x86_64-make
  3. 使用已编译好的 build\ 目录，运行 install.bat -NoBuild。
"@
        }
        Install-BuildDeps
        Add-Msys2ToPath | Out-Null
        $tools = Test-BuildReady
        if (-not $tools.Ready) {
            throw "依赖安装后仍未检测到完整工具链。请关闭并重新打开终端后再试。"
        }
    }
    Write-Ok "g++ / qmake6 / mingw32-make / windeployqt 就绪"
    Write-Step "编译 C++ 主程序（TUI + Qt GUI）"
    & cmd /c "`"$root\build.bat`""
    if ($LASTEXITCODE -ne 0) { throw "编译失败（build.bat 退出码 $LASTEXITCODE）。" }
    $output = Test-BuildOutput
    if (-not $output.Ok) {
        throw "编译完成但缺少文件：$($output.Missing -join ', ')。请检查 build.bat 输出。"
    }
}
# ---------- 3. 安装文件 ----------
Write-Step "安装到用户目录"
$build = Join-Path $root "build"
$dest = Join-Path $env:LOCALAPPDATA "MoJi\bin"
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Copy-Item (Join-Path $build "moji.exe") $dest -Force
Copy-Item (Join-Path $build "moji-gui.exe") $dest -Force
Get-ChildItem $build -Filter "*.dll" -File | Copy-Item -Destination $dest -Force
foreach ($dir in @("platforms","styles","imageformats","iconengines","tls","networkinformation","generic","platforminputcontexts")) {
    $src = Join-Path $build $dir
    if (Test-Path $src) {
        Copy-Item $src (Join-Path $dest $dir) -Recurse -Force
    }
}
$missingDest = @()
foreach ($item in @("moji.exe","moji-gui.exe","Qt6Core.dll","platforms\qwindows.dll")) {
    if (-not (Test-Path (Join-Path $dest $item))) { $missingDest += $item }
}
if ($missingDest.Count -gt 0) {
    throw "安装目录缺少文件：$($missingDest -join ', ')。"
}
Write-Ok "程序与 Qt 运行库已复制到 $dest"
# ---------- 4. PATH ----------
if ($NoPath) {
    Write-Ok "按 -NoPath 跳过 PATH 修改"
} else {
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
    Write-Ok "已把 $dest 写入用户 PATH"
} else {
    Write-Ok "用户 PATH 已包含安装目录"
}
}
$env:Path = $env:Path + ";" + $dest
# ---------- 5. 快捷方式 ----------
if ($NoShortcut) {
    Write-Ok "按 -NoShortcut 跳过快捷方式"
} else {
$shell = New-Object -ComObject WScript.Shell
$startMenu = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs"
New-Item -ItemType Directory -Force -Path $startMenu | Out-Null
$lnk = $shell.CreateShortcut((Join-Path $startMenu "墨记 MoJi.lnk"))
$lnk.TargetPath = Join-Path $dest "moji-gui.exe"
$lnk.WorkingDirectory = $dest
$lnk.Description = "墨记 MoJi 电子记事本"
$lnk.Save()
Write-Ok "开始菜单快捷方式已创建"
if ($Desktop) {
    $desktopLnk = $shell.CreateShortcut((Join-Path ([Environment]::GetFolderPath("Desktop")) "墨记 MoJi.lnk"))
    $desktopLnk.TargetPath = Join-Path $dest "moji-gui.exe"
    $desktopLnk.WorkingDirectory = $dest
    $desktopLnk.Save()
    Write-Ok "桌面快捷方式已创建"
}
}
# ---------- 6. 自检 ----------
Write-Step "安装自检"
$version = & (Join-Path $dest "moji.exe") --version 2>&1
if ($LASTEXITCODE -ne 0) { throw "moji.exe 无法启动：$version" }
Write-Ok "$version"
Write-Host ""
Write-Host "安装完成。" -ForegroundColor Green
Write-Host "  终端：moji            进入 TUI"
Write-Host "  终端：moji gui        进入 GUI"
Write-Host "  或双击开始菜单里的「墨记 MoJi」"
Write-Host "  卸载：uninstall.bat   （保留数据；加 -PurgeData 连数据一起删）"
Write-Host "数据目录：$HOME\.moji（可用 MOJI_HOME 或 --data-dir 修改）"

# 重新生成 MoJi-Setup-1.0.0.exe
# 用法：powershell -ExecutionPolicy Bypass -File installer\build-installer.ps1
param(
    [switch]$SkipBuild
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not $SkipBuild) {
    Write-Host "==> 编译 C++ 程序" -ForegroundColor Cyan
    & cmd /c "`"$root\build.bat`""
    if ($LASTEXITCODE -ne 0) { throw "build.bat 失败" }
    $c = Join-Path $root "c_legacy\notebook.exe"
    if (-not (Test-Path $c)) {
        Write-Host "==> 编译 C89 基础版" -ForegroundColor Cyan
        $gcc = "C:\msys64\ucrt64\bin\gcc.exe"
        if (-not (Test-Path $gcc)) { $gcc = (Get-Command gcc.exe -ErrorAction SilentlyContinue).Source }
        if (-not $gcc) { throw "找不到 gcc.exe，无法编译 c_legacy" }
        Push-Location (Join-Path $root "c_legacy")
        & $gcc -std=c89 -Wall -Wextra -O2 -o notebook.exe notebook.c
        Pop-Location
        if ($LASTEXITCODE -ne 0) { throw "C89 编译失败" }
    }
}

$iscc = @(
    (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"),
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $iscc) {
    Write-Host "未找到 Inno Setup 6，尝试通过 winget 安装..." -ForegroundColor Yellow
    $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
    if (-not $winget) { throw "请先安装 Inno Setup 6：https://jrsoftware.org/isdl.php" }
    & $winget.Source install --id JRSoftware.InnoSetup -e --silent --accept-package-agreements --accept-source-agreements --disable-interactivity
    $iscc = Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"
    if (-not (Test-Path $iscc)) { throw "Inno Setup 安装后仍未找到 ISCC.exe" }
}

Write-Host "==> 生成安装包" -ForegroundColor Cyan
& $iscc (Join-Path $root "installer\moji.iss")
if ($LASTEXITCODE -ne 0) { throw "ISCC 编译失败" }

$out = Join-Path $root "dist\installer\MoJi-Setup-1.0.0.exe"
if (-not (Test-Path $out)) { throw "没有找到生成的安装包：$out" }
$target = Join-Path (Split-Path -Parent $root) "MoJi-Setup-1.0.0.exe"
Copy-Item $out $target -Force
Write-Host ""
Write-Host "安装包：$target" -ForegroundColor Green
Write-Host "大小：$([math]::Round((Get-Item $target).Length / 1MB, 1)) MB"

param(
    [string]$Dir = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($Dir)) {
    $Dir = Join-Path $root "third_party\rtxui"
}
if (Test-Path (Join-Path $Dir "CMakeLists.txt")) {
    Write-Host "RTXUI 已存在：$Dir"
    exit 0
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Dir) | Out-Null
Write-Host "克隆 RTXUI 到 $Dir ..."
git clone --depth 1 https://github.com/ArthurSonzogni/RTXUI.git $Dir
Write-Host "完成。接下来："
Write-Host "  cmake -S rtxui_poc -B rtxui_poc/build -G `"MinGW Makefiles`""
Write-Host "  cmake --build rtxui_poc/build --target moji-rtxui -j4"

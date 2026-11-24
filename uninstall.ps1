# 墨记 MoJi 卸载脚本（脚本安装版）
# 用法：
#   uninstall.bat                 删除程序、PATH、快捷方式，保留数据
#   uninstall.bat -PurgeData      同时删除 %USERPROFILE%\.moji 数据
#   uninstall.bat -Yes            不询问
param(
    [switch]$PurgeData,
    [switch]$Yes
)
try { [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new() } catch { }

$ErrorActionPreference = "Stop"
function Confirm-Action([string]$question) {
    if ($Yes) { return $true }
    $answer = Read-Host "$question [y/N]"
    return ($answer -match '^(y|yes|是)$')
}
$dest = Join-Path $env:LOCALAPPDATA "MoJi\bin"
$data = Join-Path $HOME ".moji"
Write-Host "==> 关闭正在运行的墨记..." -ForegroundColor Cyan
Get-Process moji, moji-gui -ErrorAction SilentlyContinue | ForEach-Object {
    try { Stop-Process -Id $_.Id -Force -ErrorAction Stop } catch { }
}
if (Test-Path $dest) {
    Write-Host "    删除 $dest" -ForegroundColor Green
    Remove-Item $dest -Recurse -Force
} else {
    Write-Host "    未找到 $dest，跳过" -ForegroundColor Yellow
}
# 从用户 PATH 移除
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($userPath) {
    $parts = $userPath -split ';' | Where-Object {
        $_ -and ($_.TrimEnd('\') -ine $dest.TrimEnd('\'))
    }
    $newPath = ($parts -join ';')
    if ($newPath -ne $userPath) {
        [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
        Write-Host "    已从用户 PATH 移除" -ForegroundColor Green
    }
}
# 快捷方式
foreach ($lnk in @(
    (Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\墨记 MoJi.lnk"),
    (Join-Path ([Environment]::GetFolderPath("Desktop")) "墨记 MoJi.lnk")
)) {
    if (Test-Path $lnk) {
        Remove-Item $lnk -Force
        Write-Host "    已删除快捷方式 $lnk" -ForegroundColor Green
    }
}
# 数据目录
if (Test-Path $data) {
    if ($PurgeData) {
        if (Confirm-Action "将删除数据目录 $data（包含所有记事与附件），继续？") {
            Remove-Item $data -Recurse -Force
            Write-Host "    已删除数据目录 $data" -ForegroundColor Green
        } else {
            Write-Host "    保留数据目录 $data" -ForegroundColor Yellow
        }
    } else {
        Write-Host "    保留数据目录 $data（如需删除请用 uninstall.bat -PurgeData）" -ForegroundColor Yellow
    }
}
Write-Host ""
Write-Host "卸载完成。" -ForegroundColor Green
Write-Host "重新打开终端后，moji 命令将不可用。"

# 墨记 MoJi 一键安装（Windows PowerShell）
# 用法：
#   powershell -ExecutionPolicy Bypass -File .\install.ps1
#   .\install.ps1 -NoGui      # 只装终端版
#   .\install.ps1 -NoTui      # 只装图形版
param(
    [switch]$NoGui,
    [switch]$NoTui
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $root
try {
    $python = $null
    $inVenv = $false

    if ($env:VIRTUAL_ENV) {
        $candidate = Join-Path $env:VIRTUAL_ENV "Scripts\python.exe"
        if (Test-Path $candidate) {
            $python = $candidate
            $inVenv = $true
        }
    }
    if (-not $python) {
        $cmd = Get-Command python -ErrorAction SilentlyContinue
        if ($cmd) { $python = $cmd.Source }
    }
    if (-not $python) {
        throw "未找到 Python。请先安装 Python 3.9+（安装时勾选 Add Python to PATH）。"
    }

    Write-Host "使用 Python: $python"
    Write-Host "升级 pip / setuptools / wheel ..."
    & $python -m pip install --upgrade pip setuptools wheel

    if ($NoGui) {
        $extras = "tui"
    } elseif ($NoTui) {
        $extras = "gui"
    } else {
        $extras = "all"
    }

    $pipArgs = @("install")
    if (-not $inVenv) { $pipArgs += "--user" }
    $pipArgs += ".[$extras]"

    Write-Host "安装 moji-note[$extras] ..."
    & $python -m pip @pipArgs

    if ($inVenv) {
        $scripts = Join-Path $env:VIRTUAL_ENV "Scripts"
        Write-Host "已安装到虚拟环境：$scripts"
        Write-Host "请先激活虚拟环境再运行 moji。"
    } else {
        $scripts = (& $python -c "import sysconfig; print(sysconfig.get_path('scripts', 'nt_user'))").Trim()
        $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
        $already = $false
        if ($userPath) {
            foreach ($part in ($userPath -split ';')) {
                if ($part -and ($part.TrimEnd('\') -ieq $scripts.TrimEnd('\'))) {
                    $already = $true
                    break
                }
            }
        }
        if (-not $already) {
            $newPath = if ([string]::IsNullOrWhiteSpace($userPath)) { $scripts } else { $userPath.TrimEnd(';') + ";" + $scripts }
            [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
            Write-Host "已把 $scripts 写入用户 PATH。"
            Write-Host "请重新打开一个终端，或在本窗口执行："
            Write-Host "  `$env:Path += `";$scripts`""
        } else {
            Write-Host "PATH 中已包含 $scripts"
        }
        $env:Path = $env:Path + ";" + $scripts
    }

    $moji = Join-Path $scripts "moji.exe"
    if (Test-Path $moji) {
        Write-Host ""
        & $moji --version
        Write-Host ""
        Write-Host "安装完成！终端输入 moji 进入 TUI，输入 moji gui 打开图形界面。"
    } else {
        Write-Host "安装命令已完成，但没找到 moji.exe；可尝试：$python -m moji --help"
    }
    Write-Host "如果 pip 卡在系统代理，请先执行：`$env:NO_PROXY='*'"
}
finally {
    Pop-Location
}

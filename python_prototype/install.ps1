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

    # 本机的系统代理是 127.0.0.1:7890，Python 会把它误当成 HTTPS 代理，
    # 导致 pip 报 ProxyError；这里让 pip 直连。
    $env:NO_PROXY = "*"
    $env:no_proxy = "*"

    Write-Host "使用 Python: $python"
    $index = "https://pypi.tuna.tsinghua.edu.cn/simple"
    Write-Host "升级 pip / setuptools / wheel ..."
    & $python -m pip install -i $index --upgrade pip setuptools wheel
    if ($LASTEXITCODE -ne 0) {
        Write-Host "清华镜像失败，尝试默认 PyPI..."
        & $python -m pip install --upgrade pip setuptools wheel
        if ($LASTEXITCODE -ne 0) {
            throw "无法下载 pip 构建依赖。请检查网络后重试，或手动执行：`$env:NO_PROXY='*'"
        }
    }

    if ($NoGui) {
        $extras = "tui"
    } elseif ($NoTui) {
        $extras = "gui"
    } else {
        $extras = "all"
    }

    # 已经升级过 setuptools，直接用本地构建，避免构建隔离再次联网下载
    $pipArgs = @("install", "--no-build-isolation", "-i", $index)
    if (-not $inVenv) { $pipArgs += "--user" }
    $pipArgs += ".[$extras]"

    Write-Host "安装 moji-note[$extras] ..."
    & $python -m pip @pipArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "镜像安装失败，改用默认 PyPI + 构建隔离重试..."
        $retry = @("install")
        if (-not $inVenv) { $retry += "--user" }
        $retry += ".[$extras]"
        & $python -m pip @retry
        if ($LASTEXITCODE -ne 0) {
            throw "安装失败。请把完整输出发给开发者，或尝试在虚拟环境中安装（见 README）。"
        }
    }

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
}
finally {
    Pop-Location
}

# 墨记 MoJi 一键安装（Windows PowerShell）
# 用法：
#   powershell -ExecutionPolicy Bypass -File .\install.ps1
#   .\install.ps1 -NoGui      # 只装终端版
#   .\install.ps1 -NoTui      # 只装图形版
#   .\install.ps1 -VenvDir D:\moji-venv   # 指定虚拟环境位置
#
# 默认安装到独立虚拟环境 %LOCALAPPDATA%\MoJi\venv，不改动系统 Python。
# 这样可以避开全局 site-packages 中旧版 pyreadline 在 Python 3.10 上的
# `collections.Callable` 崩溃，也不会污染其他项目。
param(
    [switch]$NoGui,
    [switch]$NoTui,
    [string]$VenvDir = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $root
try {
    $basePython = $null
    $venvPython = $null
    $venvRoot = $null
    $usingExistingVenv = $false

    # 1. 如果用户已经激活了虚拟环境，就直接装进去
    if ($env:VIRTUAL_ENV) {
        $candidate = Join-Path $env:VIRTUAL_ENV "Scripts\python.exe"
        if (Test-Path $candidate) {
            $venvRoot = $env:VIRTUAL_ENV
            $venvPython = $candidate
            $usingExistingVenv = $true
        }
    }

    # 2. 否则创建/复用一个独立虚拟环境
    if (-not $venvPython) {
        $cmd = Get-Command python -ErrorAction SilentlyContinue
        if ($cmd) { $basePython = $cmd.Source }
        if (-not $basePython) {
            $py = Get-Command py -ErrorAction SilentlyContinue
            if ($py) {
                try {
                    $basePython = (& $py.Source -3 -c "import sys; print(sys.executable)").Trim()
                } catch {
                    $basePython = $null
                }
            }
        }
        if (-not $basePython) {
            throw "未找到 Python。请先安装 Python 3.9+（安装时勾选 Add Python to PATH）。"
        }

        if ([string]::IsNullOrWhiteSpace($VenvDir)) {
            $base = $env:LOCALAPPDATA
            if (-not $base) { $base = Join-Path $env:USERPROFILE "AppData\Local" }
            $VenvDir = Join-Path $base "MoJi\venv"
        }
        $VenvDir = [System.IO.Path]::GetFullPath($VenvDir)
        $venvRoot = $VenvDir
        $venvPython = Join-Path $venvRoot "Scripts\python.exe"

        if (-not (Test-Path $venvPython)) {
            Write-Host "创建独立虚拟环境：$venvRoot"
            & $basePython -m venv $venvRoot
            if ($LASTEXITCODE -ne 0 -or -not (Test-Path $venvPython)) {
                throw "创建虚拟环境失败：$venvRoot`n可手动执行：$basePython -m venv `"$venvRoot`""
            }
        } else {
            Write-Host "复用已有虚拟环境：$venvRoot"
        }
    }

    $python = $venvPython

    # 本机的系统代理是 127.0.0.1:7890，Python 会把它误当成 HTTPS 代理，
    # 导致 pip 报 ProxyError；这里让 pip 直连。
    $env:NO_PROXY = "*"
    $env:no_proxy = "*"
    # 防止用户级 site-packages 里的旧包（如 pyreadline）泄漏进虚拟环境
    $env:PYTHONNOUSERSITE = "1"

    Write-Host "使用 Python: $python"
    if (-not $usingExistingVenv) {
        Write-Host "独立环境，不会改动系统 Python。"
    }
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
    $pipArgs = @("install", "--no-build-isolation", "-i", $index, ".[$extras]")

    Write-Host "安装 moji-note[$extras] ..."
    & $python -m pip @pipArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "镜像安装失败，改用默认 PyPI + 构建隔离重试..."
        $retry = @("install", ".[$extras]")
        & $python -m pip @retry
        if ($LASTEXITCODE -ne 0) {
            throw "安装失败。请把完整输出发给开发者，或改用 -VenvDir 指定新的虚拟环境目录重试。"
        }
    }

    $scripts = Join-Path $venvRoot "Scripts"
    if ($usingExistingVenv) {
        Write-Host "已安装到当前虚拟环境：$scripts"
        Write-Host "请先激活虚拟环境再运行 moji。"
    } else {
        # 把独立环境的 Scripts 目录写入用户 PATH（只追加，不删除原有项）
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
            Write-Host "  `$env:Path = `"$scripts;`$env:Path`""
        } else {
            Write-Host "PATH 中已包含 $scripts"
        }
        $env:Path = $scripts + ";" + $env:Path
    }

    $moji = Join-Path $scripts "moji.exe"
    if (Test-Path $moji) {
        Write-Host ""
        & $moji --version
        Write-Host ""
        Write-Host "安装完成！终端输入 moji 进入 TUI，输入 moji gui 打开图形界面。"
        Write-Host "如需卸载：删除 $venvRoot，并从用户 PATH 中移除 $scripts。"
    } else {
        Write-Host "安装命令已完成，但没找到 moji.exe；可尝试：$python -m moji --help"
    }
}
finally {
    Pop-Location
}

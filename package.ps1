# 构建并打包便携版：dist\moji-cpp-1.0.0.zip
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
& cmd /c "`"$root\build.bat`""
if ($LASTEXITCODE -ne 0) { throw "编译失败" }

$stage = Join-Path $root "dist\moji-cpp-1.0.0"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item "build\moji.exe", "build\moji-gui.exe" $stage
Get-ChildItem (Join-Path $root "build") -Filter "*.dll" -File | Copy-Item -Destination $stage -Force
foreach ($dir in @("platforms","styles","imageformats","iconengines","tls","networkinformation","generic","platforminputcontexts")) {
    $src = Join-Path $root "build\$dir"
    if (Test-Path $src) { Copy-Item $src (Join-Path $stage $dir) -Recurse -Force }
}
Copy-Item "README.md" $stage -ErrorAction SilentlyContinue
Copy-Item "docs" (Join-Path $stage "docs") -Recurse -ErrorAction SilentlyContinue
$zip = Join-Path $root "dist\moji-cpp-1.0.0.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
Write-Host "已生成 $zip"

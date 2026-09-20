$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$compiler = 'E:\Program Files\mingw64\bin\g++.exe'

if (-not (Test-Path -LiteralPath $compiler)) {
    throw "未找到编译器：$compiler"
}

$build = Join-Path $root 'build'
$assets = Join-Path $build 'assets'
New-Item -ItemType Directory -Force -Path $assets | Out-Null

& $compiler `
    -std=c++17 `
    -Os `
    -s `
    -static-libgcc `
    -static-libstdc++ `
    -municode `
    -mwindows `
    (Join-Path $root 'src\main.cpp') `
    -o (Join-Path $build 'chiikawa-pet.exe') `
    -lgdiplus `
    -lole32 `
    -luuid `
    -luser32 `
    -lgdi32

if ($LASTEXITCODE -ne 0) {
    throw "编译失败，退出码：$LASTEXITCODE"
}

Copy-Item -LiteralPath (Join-Path $root 'assets\chiikawa.png') -Destination $assets -Force
Write-Output (Join-Path $build 'chiikawa-pet.exe')

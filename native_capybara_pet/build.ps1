$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$compiler = 'E:\Program Files\mingw64\bin\g++.exe'

if (-not (Test-Path -LiteralPath $compiler)) {
    throw "Compiler not found: $compiler"
}

$build = Join-Path $root 'build'
$assets = Join-Path $build 'assets'
$actionAssets = Join-Path $assets 'actions'
New-Item -ItemType Directory -Force -Path $actionAssets | Out-Null

& $compiler `
    -std=c++17 `
    -Os `
    -s `
    -static-libgcc `
    -static-libstdc++ `
    -municode `
    -mwindows `
    (Join-Path $root 'src\main.cpp') `
    -o (Join-Path $build 'capybara-lulu-pet.exe') `
    -lgdiplus `
    -lole32 `
    -luuid `
    -luser32 `
    -lgdi32

if ($LASTEXITCODE -ne 0) {
    throw "Compilation failed with exit code: $LASTEXITCODE"
}

Copy-Item -LiteralPath (Join-Path $root 'assets\lulu.png') -Destination $assets -Force
$actionNames = @('neutral.png', 'blink.png', 'wave.png', 'walk_a.png', 'walk_b.png', 'sleep.png')
foreach ($name in $actionNames) {
    Copy-Item -LiteralPath (Join-Path $root "assets\actions\$name") -Destination $actionAssets -Force
}
Write-Output (Join-Path $build 'capybara-lulu-pet.exe')

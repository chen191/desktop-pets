$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$compiler = 'E:\Program Files\mingw64\bin\g++.exe'
$windres = 'E:\Program Files\mingw64\bin\windres.exe'

if (-not (Test-Path -LiteralPath $compiler)) {
    throw "Compiler not found: $compiler"
}
if (-not (Test-Path -LiteralPath $windres)) {
    throw "Resource compiler not found: $windres"
}

$build = Join-Path $root 'build'
$actionBuild = Join-Path $build 'assets\actions'
New-Item -ItemType Directory -Force -Path $actionBuild | Out-Null
$originalPath = $env:PATH
$env:PATH = "$(Split-Path -Parent $compiler);$env:PATH"

Push-Location $root
try {
    & $windres '--preprocessor=gcc -E -xc-header -DRC_INVOKED' `
        -i 'app.rc' -O coff -o (Join-Path $build 'app-resource.o')
    if ($LASTEXITCODE -ne 0) {
        throw "Resource compilation failed with exit code: $LASTEXITCODE"
    }

    $app = Join-Path $build 'flame-keyboard-pet.exe'
    & $compiler `
        -std=c++17 `
        -Os `
        -s `
        -static-libgcc `
        -static-libstdc++ `
        -municode `
        -mwindows `
        -Wall `
        -Wextra `
        -Wpedantic `
        'src\main.cpp' `
        (Join-Path $build 'app-resource.o') `
        -o $app `
        -lgdiplus `
        -lole32 `
        -luuid `
        -luser32 `
        -lgdi32
    if ($LASTEXITCODE -ne 0) {
        throw "Application compilation failed with exit code: $LASTEXITCODE"
    }

    $unitTest = Join-Path $build 'activity_unit_test.exe'
    & $compiler -std=c++17 -O2 -Wall -Wextra -Wpedantic `
        'tests\activity_unit_test.cpp' -o $unitTest
    if ($LASTEXITCODE -ne 0) {
        throw "Unit test compilation failed with exit code: $LASTEXITCODE"
    }
    & $unitTest
    if ($LASTEXITCODE -ne 0) {
        throw "Unit test failed with exit code: $LASTEXITCODE"
    }

    $integrationTest = Join-Path $build 'keyboard_integration_test.exe'
    & $compiler -std=c++17 -O2 -municode -Wall -Wextra -Wpedantic `
        'tests\keyboard_integration_test.cpp' -o $integrationTest -luser32
    if ($LASTEXITCODE -ne 0) {
        throw "Integration test compilation failed with exit code: $LASTEXITCODE"
    }

    Get-ChildItem -LiteralPath $actionBuild -Filter '*.png' -File |
        Remove-Item -Force
    Get-ChildItem -LiteralPath (Join-Path $root 'assets\actions') -Filter '*.png' |
        Copy-Item -Destination $actionBuild -Force

    & $integrationTest $app
    if ($LASTEXITCODE -ne 0) {
        throw "Integration test failed with exit code: $LASTEXITCODE"
    }

    $package = Join-Path $root 'release\FlameKeyboardPet'
    $packageActions = Join-Path $package 'assets\actions'
    New-Item -ItemType Directory -Force -Path $packageActions | Out-Null
    Get-ChildItem -LiteralPath $packageActions -Filter '*.png' -File |
        Remove-Item -Force
    Copy-Item -LiteralPath $app -Destination $package -Force
    Get-ChildItem -LiteralPath (Join-Path $root 'assets\actions') -Filter '*.png' |
        Copy-Item -Destination $packageActions -Force
    Copy-Item -LiteralPath 'packaging\launch.bat' `
        -Destination (Join-Path $package 'START.bat') -Force
    Copy-Item -LiteralPath 'packaging\enable-startup.ps1' -Destination $package -Force
    Copy-Item -LiteralPath 'packaging\disable-startup.ps1' -Destination $package -Force
    Copy-Item -LiteralPath 'packaging\enable-startup.bat' `
        -Destination (Join-Path $package 'Enable-Startup.bat') -Force
    Copy-Item -LiteralPath 'packaging\disable-startup.bat' `
        -Destination (Join-Path $package 'Disable-Startup.bat') -Force
    Copy-Item -LiteralPath 'packaging\USER_GUIDE.md' `
        -Destination (Join-Path $package 'README.md') -Force

    $appHash = (Get-FileHash -LiteralPath $app -Algorithm SHA256).Hash
    "$appHash *flame-keyboard-pet.exe" |
        Set-Content -LiteralPath (Join-Path $package 'SHA256SUMS.txt') -Encoding ascii
    $zip = Join-Path $root 'release\FlameKeyboardPet-Windows-x64.zip'
    Compress-Archive -LiteralPath $package -DestinationPath $zip -Force

    Write-Output $app
    Write-Output $zip
} finally {
    Pop-Location
    $env:PATH = $originalPath
}

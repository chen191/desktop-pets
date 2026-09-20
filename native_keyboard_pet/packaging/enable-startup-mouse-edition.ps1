$ErrorActionPreference = 'Stop'

$exe = Join-Path $PSScriptRoot 'flame-keyboard-pet-mouse-edition.exe'
if (-not (Test-Path -LiteralPath $exe)) {
    throw 'flame-keyboard-pet-mouse-edition.exe was not found next to this script.'
}

$exe = [IO.Path]::GetFullPath($exe)
$startup = [Environment]::GetFolderPath('Startup')
$linkPath = Join-Path $startup 'Flame Keyboard Pet Mouse Edition.lnk'
$shell = New-Object -ComObject WScript.Shell
$link = $shell.CreateShortcut($linkPath)
$link.TargetPath = $exe
$link.WorkingDirectory = [IO.Path]::GetDirectoryName($exe)
$link.IconLocation = $exe + ',0'
$link.Description = 'Flame Keyboard Pet Mouse Edition - start with Windows'
$link.Save()

Write-Output $linkPath

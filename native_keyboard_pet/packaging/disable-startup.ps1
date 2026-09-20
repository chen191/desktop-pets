$ErrorActionPreference = 'Stop'

$startup = [Environment]::GetFolderPath('Startup')
$linkPath = Join-Path $startup 'Flame Keyboard Pet.lnk'
if (Test-Path -LiteralPath $linkPath) {
    Remove-Item -LiteralPath $linkPath -Force
}

Write-Output $linkPath

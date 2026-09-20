$ErrorActionPreference = 'Stop'

$startup = [Environment]::GetFolderPath('Startup')
$linkPath = Join-Path $startup 'Flame Keyboard Pet Mouse Edition.lnk'
if (Test-Path -LiteralPath $linkPath) {
    Remove-Item -LiteralPath $linkPath -Force
}

Write-Output $linkPath

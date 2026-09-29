# Packages deploy\ into dist\Prism-win64-<date>.zip for testers.
#
# The zip is self-contained: the app needs nothing installed on the target
# machine beyond Windows 10 64-bit (verified by running --selftest with a PATH
# of only C:\Windows). The installer inside is a plain PowerShell script, so
# the tester does not need admin rights or any prerequisite.
#
# Usage: powershell -File tools\package.ps1

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$deploy = Join-Path $root 'deploy'
$dist = Join-Path $root 'dist'
$installer = Join-Path $root 'tools\installer'

if (-not (Test-Path (Join-Path $deploy 'prism.exe'))) {
    throw "deploy\prism.exe missing - run tools\deploy.ps1 first"
}

# Refresh the installer files inside deploy (they are not part of the app).
foreach ($f in @('Prism-Setup.ps1', 'Install-Prism.cmd', 'README-TESTER.txt')) {
    Copy-Item (Join-Path $installer $f) $deploy -Force
}
# The old launcher is redundant next to the real installer.
Remove-Item (Join-Path $deploy 'Prism-ClickHere.cmd') -Force -ErrorAction SilentlyContinue

New-Item -ItemType Directory -Force -Path $dist | Out-Null
$zip = Join-Path $dist ("Prism-win64-{0}.zip" -f (Get-Date -Format 'yyyyMMdd'))
Remove-Item $zip -Force -ErrorAction SilentlyContinue

Write-Output "packing $zip ..."
Compress-Archive -Path (Join-Path $deploy '*') -DestinationPath $zip -CompressionLevel Optimal

$size = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Write-Output "done: $zip ($size MB)"

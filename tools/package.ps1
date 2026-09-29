# Packages deploy\ for testers:
#   dist\Prism-Setup.exe    - NSIS installer (per-user, no admin, wizard,
#                             Add/Remove Programs entry, uninstaller)
#   dist\Prism-win64-<date>.zip - portable zip (extract and run, no install)
#
# The payload is self-contained: the app needs nothing installed on the target
# machine beyond Windows 10 64-bit (verified by running --selftest with a PATH
# of only C:\Windows).
#
# Usage: powershell -File tools\package.ps1

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$deploy = Join-Path $root 'deploy'
$dist = Join-Path $root 'dist'
$installer = Join-Path $root 'tools\installer'
$makensis = 'C:\msys64\mingw64\bin\makensis.exe'

if (-not (Test-Path (Join-Path $deploy 'prism.exe'))) {
    throw "deploy\prism.exe missing - run tools\deploy.ps1 first"
}

# --- stage the payload: app files + tester README, no script installer -------
$stage = Join-Path $dist 'stage'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item "$deploy\*" $stage -Recurse -Force
foreach ($f in @('Prism-Setup.ps1', 'Install-Prism.cmd', 'Prism-ClickHere.cmd')) {
    Remove-Item (Join-Path $stage $f) -Force -ErrorAction SilentlyContinue
}
Copy-Item (Join-Path $installer 'README-TESTER.txt') $stage -Force

New-Item -ItemType Directory -Force -Path $dist | Out-Null

# --- installer ---------------------------------------------------------------
if (-not (Test-Path $makensis)) {
    throw "makensis not found at $makensis (pacman -S mingw-w64-x86_64-nsis)"
}
$setup = Join-Path $dist 'Prism-Setup.exe'
Remove-Item $setup -Force -ErrorAction SilentlyContinue
& $makensis '-V2' "-DSTAGE=$stage" (Join-Path $installer 'prism.nsi') | Write-Output
if ($LASTEXITCODE -ne 0) { throw "makensis failed (exit $LASTEXITCODE)" }
Write-Output ("installer: {0} ({1:N1} MB)" -f $setup, ((Get-Item $setup).Length / 1MB))

# --- portable zip ------------------------------------------------------------
$zip = Join-Path $dist ("Prism-win64-{0}.zip" -f (Get-Date -Format 'yyyyMMdd'))
Remove-Item $zip -Force -ErrorAction SilentlyContinue
Compress-Archive -Path "$stage\*" -DestinationPath $zip -CompressionLevel Optimal
Write-Output ("portable : {0} ({1:N1} MB)" -f $zip, ((Get-Item $zip).Length / 1MB))

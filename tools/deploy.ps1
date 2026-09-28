# Deploy build\prism.exe to deploy\ safely.
#
# A running Prism locks deploy\prism.exe, and Copy-Item then fails with an
# IOException. That failure is easy to miss in a long build log, and the result
# is a stale executable that "does not have" the feature that was just written —
# so this script kills the app, retries the copy, and *verifies with a hash*
# instead of assuming the copy worked.
#
# Usage: pwsh -File tools\deploy.ps1

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'build\prism.exe'
$target = Join-Path $root 'deploy\prism.exe'

if (-not (Test-Path $source)) {
    throw "Not built yet: $source (run: cmake --build --preset default)"
}

Get-Process prism -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800

for ($attempt = 1; $attempt -le 10; $attempt++) {
    try {
        Copy-Item $source $target -Force -ErrorAction Stop
        break
    } catch {
        if ($attempt -eq 10) { throw }
        Start-Sleep -Milliseconds 500
    }
}

$built = (Get-FileHash $source).Hash
$deployed = (Get-FileHash $target).Hash
if ($built -ne $deployed) {
    throw "Deploy mismatch: build=$built deploy=$deployed"
}

Write-Output "deployed $target ($($deployed.Substring(0, 12)))"

# Deploy the RELEASE build to deploy\ safely, together with the Qt plugins the
# app needs at runtime.
#
# The release preset is what ships: the default preset is a Debug build with no
# optimisation, and shipping that is why testers reported the app as "very
# laggy" — Release is ~5x faster on every measured path (stroke, commit, fill).
#
# Two failure modes this script exists to prevent:
#
# 1. A running Prism locks deploy\prism.exe, so Copy-Item fails with an
#    IOException that is easy to lose in a long build log. The result is a stale
#    executable that "does not have" the change just made, which reads as a
#    broken feature rather than a failed copy. -> kill, retry, verify hash.
# 2. Qt ships PNG in QtGui but JPEG/GIF/ICO as *plugins*. Without
#    plugins\imageformats the app silently cannot open or save those formats —
#    a .jpg import just reports "unsupported or unreadable image". -> copy the
#    image format plugins and their non-Qt dependencies every deploy.
#
# Usage: powershell -File tools\deploy.ps1

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'build-release\prism.exe'
$target = Join-Path $root 'deploy\prism.exe'

$qtPlugins = 'C:\msys64\ucrt64\share\qt6\plugins'
$msysBin = 'C:\msys64\ucrt64\bin'

if (-not (Test-Path $source)) {
    throw "Not built yet: $source (run: cmake --preset release; cmake --build --preset release)"
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

# Image format plugins. Without them only PNG works: JPEG/GIF/ICO are plugins.
$imageFormats = Join-Path $target '..\plugins\imageformats'
New-Item -ItemType Directory -Force -Path $imageFormats | Out-Null
foreach ($plugin in @('qjpeg.dll', 'qgif.dll', 'qico.dll')) {
    $from = Join-Path $qtPlugins "imageformats\$plugin"
    if (Test-Path $from) {
        Copy-Item $from $imageFormats -Force
    } else {
        Write-Warning "missing Qt plugin: $from"
    }
}

# Non-Qt libraries the plugins link against (qjpeg -> libjpeg-8).
foreach ($lib in @('libjpeg-8.dll')) {
    $from = Join-Path $msysBin $lib
    if (Test-Path $from) {
        Copy-Item $from (Join-Path $target '..') -Force
    } else {
        Write-Warning "missing runtime library: $from"
    }
}

# Fail loudly if the deployed build cannot actually read/write JPEG, instead of
# discovering it when a user imports a photo.
#
# Prism is a GUI-subsystem binary: PowerShell does not wait for it and never
# sets $LASTEXITCODE, so the exit code must come from -Wait -PassThru. Its
# diagnostics land in %APPDATA%\Prism\Prism\prism.log, not on stdout.
$probe = Start-Process -FilePath $target -ArgumentList '--check-formats' -Wait -PassThru
if ($probe.ExitCode -ne 0) {
    $log = Join-Path $env:APPDATA 'Prism\Prism\prism.log'
    if (Test-Path $log) {
        Get-Content $log | Select-Object -Last 5 | ForEach-Object { Write-Output $_ }
    }
    throw "Deployed build is missing image formats (exit $($probe.ExitCode))"
}

Write-Output "deployed $target ($($deployed.Substring(0, 12)))"

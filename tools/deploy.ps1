# Fresh dynamic Qt Release deployment. Distribution licenses are gated separately.
param([string]$MsysRoot = 'C:\msys64')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'package-validation.ps1')
$root = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$source = Join-Path $root 'build-release\prism.exe'
$deploy = Join-Path $root 'deploy'
$bin = Join-Path $MsysRoot 'ucrt64\bin'
$plugins = Join-Path $MsysRoot 'ucrt64\share\qt6\plugins'
if (-not (Test-Path -LiteralPath $source)) { throw 'Build Release first: cmake --preset release; cmake --build --preset release' }
$env:PATH = "$bin;$env:PATH"
# Rebuild through the preset so passing tests cannot bless a stale executable.
Push-Location $root
try {
    & (Join-Path $bin 'cmake.exe') --build --preset release
    if ($LASTEXITCODE -ne 0) { throw "Release build failed: $LASTEXITCODE" }
} finally { Pop-Location }
$identity = Get-PrismIdentity $root
$releaseHash = Get-PrismHash $source
$stage = Join-Path $root ('build\deploy-stage-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item -LiteralPath $source -Destination (Join-Path $stage 'prism.exe')
& (Join-Path $bin 'windeployqt.exe') --release --force --no-patchqt --no-translations --no-opengl-sw --no-system-d3d-compiler --no-ffmpeg --compiler-runtime --qmldir (Join-Path $root 'src\ui\qml') --qmlimport (Join-Path $root 'build-release') --dir $stage (Join-Path $stage 'prism.exe')
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed: $LASTEXITCODE" }
$module = Join-Path $stage 'qml\Prism'
New-Item -ItemType Directory -Force -Path $module | Out-Null
Copy-Item -Path (Join-Path $root 'build-release\Prism\*') -Destination $module -Recurse -Force
# Preserve installed image formats; licensing must never silently remove TIFF.
$formats = Join-Path $stage 'plugins\imageformats'
New-Item -ItemType Directory -Force -Path $formats | Out-Null
Copy-Item -Path (Join-Path $plugins 'imageformats\q*.dll') -Destination $formats -Force
[IO.File]::WriteAllText((Join-Path $stage 'qt.conf'), "[Paths]`nPlugins=plugins`nQml2Imports=qml`n", [Text.UTF8Encoding]::new($false))
# Walk PE imports, including every codec plugin's transitive runtime dependency.
$queue = [Collections.Generic.Queue[string]]::new()
$seen = @{}
foreach ($file in Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object Extension -in @('.exe', '.dll')) { $queue.Enqueue($file.FullName) }
while ($queue.Count) {
    $file = $queue.Dequeue()
    if ($seen.ContainsKey($file)) { continue }
    $seen[$file] = $true
    $imports = @(& (Join-Path $bin 'objdump.exe') -p $file)
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect PE imports: $file" }
    foreach ($line in $imports) {
        if ($line -notmatch 'DLL Name:\s*(\S+)') { continue }
        $name = $Matches[1]
        $installed = Join-Path $bin $name
        if (Test-Path -LiteralPath $installed) {
            $destination = Join-Path $stage $name
            if (-not (Test-Path -LiteralPath $destination)) { Copy-Item -LiteralPath $installed -Destination $destination }
            if ((Get-PrismHash $destination) -ne (Get-PrismHash $installed)) { throw "Runtime DLL mismatch: $name" }
            $queue.Enqueue($destination)
        } elseif ($name -notmatch '^(api-ms-|ext-ms-)' -and -not (Test-Path -LiteralPath (Join-Path $env:SystemRoot "System32\$name"))) {
            throw "Unresolved runtime import $name in $file"
        }
    }
}
foreach ($relative in @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Quick.dll', 'plugins\platforms\qwindows.dll', 'plugins\imageformats\qjpeg.dll', 'plugins\imageformats\qwebp.dll', 'plugins\imageformats\qtiff.dll', 'qml\Prism\qmldir')) {
    if (-not (Test-Path -LiteralPath (Join-Path $stage $relative))) { throw "Missing deployment file: $relative" }
}
& (Join-Path $bin 'ctest.exe') --test-dir (Join-Path $root 'build-release') --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Release tests failed: $LASTEXITCODE" }
$originalPath = $env:PATH
$originalRecovery = $env:PRISM_RECOVERY_DIR
$originalPlatform = $env:QT_QPA_PLATFORM
$originalSettings = $env:PRISM_SETTINGS_DIR
try {
    # Runtime closure only: this does not establish clean-install or tablet acceptance.
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $env:PRISM_RECOVERY_DIR = Join-Path $stage 'validation-recovery'
    $env:PRISM_SETTINGS_DIR = Join-Path $stage 'validation-settings'
    $env:QT_QPA_PLATFORM = 'offscreen'
    $versionOutput = Join-Path $stage 'validation-version.txt'
    $versionProbe = Start-Process -FilePath (Join-Path $stage 'prism.exe') -ArgumentList '--version' -RedirectStandardOutput $versionOutput -Wait -PassThru -WindowStyle Hidden
    if ($versionProbe.ExitCode -ne 0 -or (Get-Content -LiteralPath $versionOutput -Raw).Trim() -ne $identity.version) { throw 'Compiled version differs from PrismVersion.txt' }
    Remove-Item -LiteralPath $versionOutput
    foreach ($argument in @('--check-formats', '--selftest')) {
        $probe = Start-Process -FilePath (Join-Path $stage 'prism.exe') -ArgumentList $argument -WorkingDirectory (Join-Path $root 'build-release') -Wait -PassThru -WindowStyle Hidden
        if ($probe.ExitCode -ne 0) { throw "Staged $argument failed: $($probe.ExitCode); inspect %APPDATA%\Prism\Prism\prism.log" }
    }
} finally { $env:PATH = $originalPath; $env:PRISM_RECOVERY_DIR = $originalRecovery; $env:QT_QPA_PLATFORM = $originalPlatform; $env:PRISM_SETTINGS_DIR = $originalSettings }
foreach ($folder in @('validation-recovery', 'validation-settings')) {
    $temporary = [IO.Path]::GetFullPath((Join-Path $stage $folder))
    if (-not $temporary.StartsWith($stage + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid validation directory path' }
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
$after = Get-PrismIdentity $root
if ($identity.sourceSha256 -ne $after.sourceSha256 -or $identity.gitCommit -ne $after.gitCommit -or $releaseHash -ne (Get-PrismHash $source)) { throw 'Source or Release changed during validation; rebuild and redeploy' }
$manifest = [ordered]@{ schemaVersion = 1; version = $identity.version; gitCommit = $identity.gitCommit; gitDirty = $identity.gitDirty; sourceSha256 = $identity.sourceSha256; releaseExeSha256 = $releaseHash; testedUtc = [DateTime]::UtcNow.ToString('o'); checks = @('compiled --version', 'Release ctest', 'staged --check-formats', 'staged --selftest with Windows-only PATH'); acceptance = 'Physical tablet, eight-hour drawing and clean-machine install/upgrade/uninstall NOT verified'; payload = Get-PrismPayload $stage }
Write-PrismJson (Join-Path $stage 'candidate-manifest.json') $manifest
Assert-PrismPayload $stage $manifest $source
Get-Process prism -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq (Join-Path $deploy 'prism.exe') } | Stop-Process -Force
# Preserve files somebody may have saved in the previous development directory.
if (Test-Path -LiteralPath $deploy) {
    $backup = Join-Path $root ('build\deploy-previous-' + [Guid]::NewGuid().ToString('N'))
    if (-not ([IO.Path]::GetFullPath($deploy)).StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase) -or -not $backup.StartsWith($root + '\build\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Deployment paths escaped the workspace' }
    Move-Item -LiteralPath $deploy -Destination $backup
    Write-Output "Previous deployment retained at $backup"
}
Move-Item -LiteralPath $stage -Destination $deploy
Assert-PrismPayload $deploy $manifest $source
Write-Output "Deployed $($identity.version) ($releaseHash). Distribution license review is a separate packaging gate."

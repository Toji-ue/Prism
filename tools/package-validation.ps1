# Shared candidate identity, integrity and dependency provenance checks.
# Dot-source this file from deployment/packaging; -SelfTest uses isolated fixtures.
param([switch]$SelfTest)
$ErrorActionPreference = 'Stop'

function Get-PrismHash([string]$Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Write-PrismJson([string]$Path, $Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
}

function Get-PrismIdentity([string]$Root) {
    $version = (Get-Content -LiteralPath (Join-Path $Root 'PrismVersion.txt') -Raw).Trim()
    if ($version -notmatch '^\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?$') { throw 'Invalid PrismVersion.txt' }
    $commit = (& git -C $Root rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve Git HEAD' }
    $status = @(& git -C $Root status --porcelain --untracked-files=all)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read Git status' }
    $paths = @(& git -C $Root ls-files --cached --others --exclude-standard -- src tests tools cmake docs third_party .github PrismVersion.txt AGENTS.md CMakeLists.txt CMakePresets.json vcpkg.json vcpkg-configuration.json .clang-format .gitattributes)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot enumerate source identity' }
    $entries = foreach ($path in ($paths | Sort-Object -Unique)) {
        $file = Join-Path $Root $path
        if (Test-Path -LiteralPath $file -PathType Leaf) { "$path $(Get-PrismHash $file)" }
        else { "$path missing" }
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $fingerprint = [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($entries -join "`n")))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
    [ordered]@{ version = $version; gitCommit = $commit; gitDirty = ($status.Count -gt 0); sourceSha256 = $fingerprint }
}

function Get-PrismPayload([string]$Directory) {
    $base = [IO.Path]::GetFullPath($Directory).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $ownManifest = Join-Path $Directory 'candidate-manifest.json'
    $ownManifest = [IO.Path]::GetFullPath($ownManifest)
    @(Get-ChildItem -LiteralPath $Directory -File -Recurse | Where-Object FullName -ne $ownManifest | Sort-Object FullName | ForEach-Object {
        [ordered]@{ path = $_.FullName.Substring($base.Length).Replace('\', '/'); bytes = $_.Length; sha256 = Get-PrismHash $_.FullName }
    })
}

function Assert-PrismPayload([string]$Directory, $Manifest, [string]$ReleaseExe) {
    if ((Get-PrismHash $ReleaseExe) -ne $Manifest.releaseExeSha256) { throw 'Tested Release executable changed' }
    if ((Get-PrismHash (Join-Path $Directory 'prism.exe')) -ne $Manifest.releaseExeSha256) { throw 'Payload executable differs from tested Release' }
    $actual = @(Get-PrismPayload $Directory)
    if ($actual.Count -ne @($Manifest.payload).Count) { throw 'Payload file count changed' }
    $expected = @{}
    foreach ($entry in $Manifest.payload) {
        if ($expected.ContainsKey($entry.path)) { throw "Duplicate manifest path: $($entry.path)" }
        $expected[$entry.path] = $entry
    }
    foreach ($entry in $actual) {
        $known = $expected[$entry.path]
        if (-not $known -or $known.sha256 -ne $entry.sha256 -or $known.bytes -ne $entry.bytes) { throw "Payload mismatch: $($entry.path)" }
    }
}

function Assert-PrismZip([string]$Zip, $Manifest) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead($Zip)
    try {
        $allFiles = @($archive.Entries | Where-Object Name -ne '')
        if ($allFiles.Count -ne @($Manifest.payload).Count + 1) { throw 'ZIP entry count differs from staged payload plus manifest' }
        $files = @($allFiles | Where-Object FullName -ne 'candidate-manifest.json')
        if ($files.Count -ne @($Manifest.payload).Count) { throw 'ZIP file count differs from staged payload' }
        $expected = @{}
        foreach ($entry in $Manifest.payload) { $expected[$entry.path] = $entry }
        $seen = @{}
        foreach ($entry in $files) {
            $path = $entry.FullName.Replace('\', '/')
            if ($seen.ContainsKey($path)) { throw "Duplicate ZIP entry: $path" }
            $seen[$path] = $true
            $known = $expected[$path]
            if (-not $known -or $known.bytes -ne $entry.Length) { throw "Unexpected ZIP entry: $path" }
            $stream = $entry.Open(); $sha = [Security.Cryptography.SHA256]::Create()
            try { $hash = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
            finally { $stream.Dispose(); $sha.Dispose() }
            if ($hash -ne $known.sha256) { throw "ZIP content mismatch: $path" }
        }
        $manifestEntry = $archive.GetEntry('candidate-manifest.json')
        if (-not $manifestEntry) { throw 'ZIP candidate manifest missing' }
        $reader = [IO.StreamReader]::new($manifestEntry.Open())
        try { $archivedManifest = $reader.ReadToEnd() | ConvertFrom-Json }
        finally { $reader.Dispose() }
        if (($archivedManifest | ConvertTo-Json -Depth 12 -Compress) -ne ($Manifest | ConvertTo-Json -Depth 12 -Compress)) { throw 'ZIP manifest differs from staged manifest' }
    } finally { $archive.Dispose() }
}

function Read-PrismMsysPackages([string]$MsysRoot) {
    foreach ($dir in Get-ChildItem -LiteralPath (Join-Path $MsysRoot 'var\lib\pacman\local') -Directory -Filter 'mingw-w64-ucrt-x86_64-*') {
        $values = @{}; $key = ''
        foreach ($line in Get-Content -LiteralPath (Join-Path $dir.FullName 'desc')) {
            if ($line -match '^%(.+)%$') { $key = $Matches[1]; $values[$key] = @() }
            elseif ($line -ne '' -and $key) { $values[$key] += $line }
        }
        if ($values.NAME[0] -like '*-debug') { continue }
        $files = @(Get-Content -LiteralPath (Join-Path $dir.FullName 'files') | Where-Object { $_ -notmatch '^%' -and $_ -ne '' -and $_ -notmatch '/$' })
        [pscustomobject]@{ name = $values.NAME[0]; version = $values.VERSION[0]; license = $values.LICENSE -join ' '; upstream = $values.URL[0]; packageBase = $values.BASE[0]; description = $values.DESC[0]; files = $files; metadata = Join-Path $dir.FullName 'desc' }
    }
}

function Export-PrismNotices([string]$Payload, [string]$Output, [string]$MsysRoot = 'C:\msys64') {
    New-Item -ItemType Directory -Force -Path $Output | Out-Null
    $packages = @(Read-PrismMsysPackages $MsysRoot)
    $dlls = @{}
    foreach ($package in $packages) {
        foreach ($path in $package.files | Where-Object { $_ -like '*.dll' }) {
            $name = [IO.Path]::GetFileName($path).ToLowerInvariant()
            if (-not $dlls.ContainsKey($name)) { $dlls[$name] = @() }
            $dlls[$name] += [pscustomobject]@{ package = $package; path = $path }
        }
    }
    $used = @{}; $runtimes = @(); $unmatched = @()
    $base = [IO.Path]::GetFullPath($Payload).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    foreach ($file in Get-ChildItem -LiteralPath $Payload -File -Recurse -Filter '*.dll') {
        $hash = Get-PrismHash $file.FullName
        $match = $null
        foreach ($candidate in $dlls[$file.Name.ToLowerInvariant()]) {
            $source = Join-Path $MsysRoot $candidate.path
            if ((Test-Path -LiteralPath $source) -and (Get-PrismHash $source) -eq $hash) { $match = $candidate; break }
        }
        $relative = $file.FullName.Substring($base.Length).Replace('\', '/')
        if (-not $match) { $unmatched += $relative; continue }
        $used[$match.package.name] = $match.package
        $runtimes += [ordered]@{ path = $relative; sha256 = $hash; package = $match.package.name; sourcePath = $match.path }
    }
    $inventory = @()
    foreach ($package in $used.Values | Sort-Object name) {
        $folder = Join-Path $Output $package.name
        New-Item -ItemType Directory -Force -Path $folder | Out-Null
        Copy-Item -LiteralPath $package.metadata -Destination (Join-Path $folder 'pacman-desc.txt') -Force
        $licensePaths = @($package.files | Where-Object { $_ -match '/share/licenses/' -or $_ -match '/(?:LICENSE(?:\.txt)?|COPYING(?:\.[^/]+)?)$' })
        foreach ($path in $licensePaths) {
            $source = Join-Path $MsysRoot $path
            if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { continue }
            # Preserve paths; Qt packages contain several distinct license texts.
            $destination = Join-Path $folder $path
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination -Force
        }
        $status = 'recorded-permissive'
        if ($package.name -like '*-jbigkit') { $status = 'blocked-GPL-signoff-required' }
        elseif ($package.license -match 'LGPL|MPL|GPL|LicenseRef|custom') { $status = 'distribution-review-required' }
        if ($licensePaths.Count -eq 0) { $status = 'missing-license-text' }
        $inventory += [ordered]@{ name = $package.name; version = $package.version; licenseFromPacman = $package.license; upstream = $package.upstream; recipe = "https://github.com/msys2/MINGW-packages/tree/master/$($package.packageBase)"; review = $status; licenseFiles = $licensePaths }
    }
    $result = [ordered]@{ schemaVersion = 1; recordedUtc = [DateTime]::UtcNow.ToString('o'); origin = 'Installed MSYS2 pacman database and byte-matched DLLs, not a compliance certification'; packages = $inventory; runtimeFiles = $runtimes; unmatchedDlls = $unmatched }
    Write-PrismJson (Join-Path $Output 'dependency-inventory.json') $result
    return $result
}

function Assert-PrismDistributionLicenses($Inventory) {
    $issues = @($Inventory.packages | Where-Object review -ne 'recorded-permissive' | ForEach-Object { "$($_.name) $($_.version): $($_.review) [$($_.licenseFromPacman)]" })
    $issues += @($Inventory.unmatchedDlls | ForEach-Object { "Unknown DLL provenance: $_" })
    if ($issues.Count -gt 0) { throw ("Candidate distribution is blocked. Review the staged THIRD-PARTY inventory and exact license texts:`n" + ($issues -join "`n")) }
}

if ($SelfTest) {
    $fixture = Join-Path $PSScriptRoot '..\build\package-validation-fixture'
    $fixture = [IO.Path]::GetFullPath($fixture)
    $allowed = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\build')).TrimEnd('\') + '\'
    if (-not $fixture.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture must remain inside build' }
    if (Test-Path -LiteralPath $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force }
    New-Item -ItemType Directory -Force -Path (Join-Path $fixture 'payload\plugins') | Out-Null
    [IO.File]::WriteAllText((Join-Path $fixture 'release.exe'), 'tested release')
    Copy-Item -LiteralPath (Join-Path $fixture 'release.exe') -Destination (Join-Path $fixture 'payload\prism.exe')
    [IO.File]::WriteAllText((Join-Path $fixture 'payload\plugins\plugin.dll'), 'plugin bytes')
    $manifest = [ordered]@{ version = '0.3.0-beta.1'; releaseExeSha256 = Get-PrismHash (Join-Path $fixture 'release.exe'); payload = Get-PrismPayload (Join-Path $fixture 'payload') }
    Write-PrismJson (Join-Path $fixture 'payload\candidate-manifest.json') $manifest
    Assert-PrismPayload (Join-Path $fixture 'payload') $manifest (Join-Path $fixture 'release.exe')
    Compress-Archive -Path (Join-Path $fixture 'payload\*') -DestinationPath (Join-Path $fixture 'candidate.zip')
    Assert-PrismZip (Join-Path $fixture 'candidate.zip') $manifest
    $changedZip = [IO.Compression.ZipFile]::Open((Join-Path $fixture 'candidate.zip'), [IO.Compression.ZipArchiveMode]::Update)
    try {
        $entry = $changedZip.Entries | Where-Object { $_.FullName.Replace('\', '/') -eq 'plugins/plugin.dll' } | Select-Object -First 1
        $stream = $entry.Open()
        try { $bytes = [Text.Encoding]::UTF8.GetBytes('other  bytes'); $stream.Write($bytes, 0, $bytes.Length) }
        finally { $stream.Dispose() }
    } finally { $changedZip.Dispose() }
    $zipRejected = $false
    try { Assert-PrismZip (Join-Path $fixture 'candidate.zip') $manifest } catch { $zipRejected = $true }
    if (-not $zipRejected) { throw 'Tampered ZIP was accepted' }
    [IO.File]::WriteAllText((Join-Path $fixture 'payload\plugins\plugin.dll'), 'changed plugin')
    $rejected = $false
    try { Assert-PrismPayload (Join-Path $fixture 'payload') $manifest (Join-Path $fixture 'release.exe') } catch { $rejected = $true }
    if (-not $rejected) { throw 'Tampered payload was accepted' }
    $blocked = $false
    try { Assert-PrismDistributionLicenses @{ packages = @(@{ name = 'jbigkit'; version = '2.1'; review = 'blocked-GPL-signoff-required'; licenseFromPacman = 'GPL-2.0' }); unmatchedDlls = @() } } catch { $blocked = $true }
    if (-not $blocked) { throw 'Blocked dependency was accepted' }
    Write-Output 'PASS: staged/ZIP hashes, modified payload/ZIP rejection, GPL distribution gate'
}

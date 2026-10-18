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
        # Get-Content attaches PSDrive/provider graphs to strings on Windows PS 5.1;
        # serializing those expands a tiny inventory into gigabytes of metadata.
        foreach ($line in [IO.File]::ReadAllLines((Join-Path $dir.FullName 'desc'))) {
            if ($line -match '^%(.+)%$') { $key = $Matches[1]; $values[$key] = @() }
            elseif ($line -ne '' -and $key) { $values[$key] += $line }
        }
        if ($values.NAME[0] -like '*-debug') { continue }
        $files = @([IO.File]::ReadAllLines((Join-Path $dir.FullName 'files')) | Where-Object { $_ -notmatch '^%' -and $_ -ne '' -and $_ -notmatch '/$' })
        [pscustomobject]@{ name = $values.NAME[0]; version = $values.VERSION[0]; license = $values.LICENSE -join ' '; upstream = $values.URL[0]; packageBase = $values.BASE[0]; description = $values.DESC[0]; files = $files; metadata = Join-Path $dir.FullName 'desc' }
    }
}

function Assert-PrismTiffProvenance([string]$Payload) {
    $notice = Join-Path $Payload 'tiff-runtime'
    $manifest = Get-Content -LiteralPath (Join-Path $notice 'manifest.json') -Raw | ConvertFrom-Json
    if ($manifest.schemaVersion -ne 1 -or $manifest.version -ne '4.7.2' -or $manifest.jbig -ne $false) { throw 'Unsupported TIFF build provenance' }
    $sourceHash = '672bd7d10aee4606171afb864f3570b83340f6a33e2c186dc0512f7145ffdf6a'
    $licenseHash = '0e27c2382d7b8147972bbb746e04059a1152c8d0fda9d03ef1399d1a433c4ade'
    if ($manifest.sourceSha256 -ne $sourceHash -or (Get-PrismHash (Join-Path $notice 'tiff-4.7.2.tar.gz')) -ne $sourceHash) { throw 'TIFF source provenance mismatch' }
    if ($manifest.licenseSha256 -ne $licenseHash -or (Get-PrismHash (Join-Path $notice 'LICENSE.md')) -ne $licenseHash) { throw 'TIFF license mismatch' }
    $recipeHash = Get-PrismHash (Join-Path $PSScriptRoot 'build-tiff-runtime.ps1')
    if ($manifest.recipeSha256 -ne $recipeHash -or (Get-PrismHash (Join-Path $notice 'build-tiff-runtime.ps1')) -ne $recipeHash) { throw 'TIFF build recipe mismatch' }
    if ((Get-PrismHash (Join-Path $Payload 'libtiff-6.dll')) -ne $manifest.dllSha256) { throw 'TIFF runtime hash mismatch' }
    return $manifest
}

function Read-PrismDistributionReview {
    $path = Join-Path $PSScriptRoot '..\third_party\distribution-review.json'
    $review = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    if ($review.schemaVersion -ne 1 -or $review.platform -ne 'windows-ucrt64') { throw 'Unsupported distribution review' }
    return $review
}

function Find-PrismRuntimeReview($Package, $RuntimeFiles, [string]$MsysRoot, $Review) {
    $matches = @($Review.runtimeReviews | Where-Object { $_.package -eq $Package.name -and $_.version -eq $Package.version })
    if ($matches.Count -ne 1) { return $null }
    $entry = $matches[0]
    $actual = @($RuntimeFiles | Where-Object package -eq $Package.name)
    foreach ($file in $actual) {
        if ($entry.runtimeFiles -notcontains $file.path) { return $null }
    }
    if (@($entry.licenseFiles).Count -eq 0) { return $null }
    foreach ($license in $entry.licenseFiles) {
        if ($Package.files -notcontains $license.path) { return $null }
        $path = Join-Path $MsysRoot $license.path
        if (-not (Test-Path -LiteralPath $path) -or (Get-PrismHash $path) -ne $license.sha256) { return $null }
    }
    return $entry
}

function Copy-PrismCorrespondingSources($Inventory, [string]$Destination) {
    $review = Read-PrismDistributionReview
    New-Item -ItemType Directory -Force $Destination | Out-Null
    $cache = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\build\corresponding-sources'))
    New-Item -ItemType Directory -Force $cache | Out-Null
    foreach ($name in @($Inventory.packages | Where-Object sourcePackage | ForEach-Object sourcePackage | Sort-Object -Unique)) {
        if ($name -notmatch '^mingw-w64-[a-z0-9.+-]+\.src\.tar\.zst$') { throw 'Invalid corresponding-source name' }
        $sources = @($review.sources | Where-Object file -eq $name)
        if ($sources.Count -ne 1) { throw "Missing source review: $name" }
        $entry = $sources[0]
        foreach ($suffix in @('', '.sig')) {
            $file = Join-Path $cache ($name + $suffix)
            if (-not (Test-Path -LiteralPath $file)) {
                Invoke-WebRequest -UseBasicParsing -Uri ("https://repo.msys2.org/mingw/sources/" + $name + $suffix) -OutFile $file
            }
            $expected = if ($suffix) { $entry.signatureSha256 } else { $entry.sha256 }
            if ((Get-PrismHash $file) -ne $expected) { throw "Corresponding source checksum mismatch: $name$suffix" }
            Copy-Item -LiteralPath $file -Destination $Destination -Force
        }
    }
    $key = Join-Path $PSScriptRoot '..\third_party\msys2-source-signing-key.asc'
    if ((Get-PrismHash $key) -ne $review.publicKeySha256) { throw 'Source signing key changed since review' }
    Copy-Item -LiteralPath $key -Destination $Destination -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '..\third_party\distribution-review.json') -Destination $Destination -Force
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
    $customTiff = $null
    foreach ($file in Get-ChildItem -LiteralPath $Payload -File -Recurse -Filter '*.dll') {
        $hash = Get-PrismHash $file.FullName
        $relative = $file.FullName.Substring($base.Length).Replace('\', '/')
        if ($relative -ieq 'libtiff-6.dll' -and (Test-Path -LiteralPath (Join-Path $Payload 'tiff-runtime\manifest.json'))) {
            $customTiff = Assert-PrismTiffProvenance $Payload
            $runtimes += [ordered]@{ path = $relative; sha256 = $hash; package = 'prism-libtiff-no-jbig'; sourcePath = 'tiff-runtime/tiff-4.7.2.tar.gz' }
            continue
        }
        $match = $null
        foreach ($candidate in $dlls[$file.Name.ToLowerInvariant()]) {
            $source = Join-Path $MsysRoot $candidate.path
            if ((Test-Path -LiteralPath $source) -and (Get-PrismHash $source) -eq $hash) { $match = $candidate; break }
        }
        if (-not $match) { $unmatched += $relative; continue }
        $used[$match.package.name] = $match.package
        $runtimes += [ordered]@{ path = $relative; sha256 = $hash; package = $match.package.name; sourcePath = $match.path }
    }
    $inventory = @()
    $distributionReview = Read-PrismDistributionReview
    if ($customTiff) {
        $folder = Join-Path $Output 'prism-libtiff-no-jbig'
        New-Item -ItemType Directory -Force $folder | Out-Null
        Copy-Item -Path (Join-Path $Payload 'tiff-runtime\*') -Destination $folder -Force
        $inventory += [ordered]@{ name = 'prism-libtiff-no-jbig'; version = $customTiff.version; licenseFromPacman = 'MIT (pinned upstream LICENSE.md, custom build)'; upstream = 'https://libtiff.gitlab.io/libtiff/'; recipe = 'build-tiff-runtime.ps1'; review = 'recorded-permissive'; licenseFiles = @('LICENSE.md'); limitation = $customTiff.limitation }
    }
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
        $status = 'distribution-review-required'
        $componentReview = Find-PrismRuntimeReview $package $runtimes $MsysRoot $distributionReview
        $sourcePackage = $null
        $selectedLicense = $null
        if ($componentReview) {
            $sourcePackage = $componentReview.sourcePackage
            $selectedLicense = $componentReview.selectedLicense
            $status = if ($sourcePackage) { 'reviewed-LGPL-source-required' } else { 'reviewed-permissive-or-runtime-exception' }
            if ($componentReview.requiresGplExceptionSignoff -and -not $componentReview.gplExceptionSignoff) {
                $status = 'blocked-GPL-exception-signoff-required'
            }
        }
        if ($package.name -like '*-jbigkit') { $status = 'blocked-GPL-signoff-required' }
        if ($licensePaths.Count -eq 0) { $status = 'missing-license-text' }
        $inventory += [ordered]@{ name = $package.name; version = $package.version; licenseFromPacman = $package.license; selectedLicense = $selectedLicense; sourcePackage = $sourcePackage; upstream = $package.upstream; recipe = "https://github.com/msys2/MINGW-packages/tree/master/$($package.packageBase)"; review = $status; licenseFiles = $licensePaths }
    }
    $result = [ordered]@{ schemaVersion = 1; recordedUtc = [DateTime]::UtcNow.ToString('o'); origin = 'Byte-matched MSYS2 DLLs and hash-validated custom TIFF provenance, not a compliance certification'; packages = $inventory; runtimeFiles = $runtimes; unmatchedDlls = $unmatched }
    Write-PrismJson (Join-Path $Output 'dependency-inventory.json') $result
    return $result
}

function Assert-PrismDistributionLicenses($Inventory, [string]$Sources = '') {
    $review = Read-PrismDistributionReview
    $issues = @()
    foreach ($package in $Inventory.packages) {
        if ($package.review -in @('recorded-permissive', 'reviewed-permissive-or-runtime-exception')) { continue }
        if ($package.review -eq 'reviewed-LGPL-source-required' -and $Sources) {
            $entries = @($review.sources | Where-Object file -eq $package.sourcePackage)
            if ($entries.Count -eq 1) {
                $entry = $entries[0]
                $file = Join-Path $Sources $entry.file
                $signature = $file + '.sig'
                if ((Test-Path -LiteralPath $file) -and (Test-Path -LiteralPath $signature) -and
                    (Get-PrismHash $file) -eq $entry.sha256 -and (Get-PrismHash $signature) -eq $entry.signatureSha256) { continue }
            }
        }
        $issues += "$($package.name) $($package.version): $($package.review) [$($package.licenseFromPacman)]"
    }
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
    $pacman = Join-Path $fixture 'var\lib\pacman\local\mingw-w64-ucrt-x86_64-fixture-1'
    New-Item -ItemType Directory -Force $pacman | Out-Null
    [IO.File]::WriteAllText((Join-Path $pacman 'desc'), "%NAME%`nmingw-w64-ucrt-x86_64-fixture`n`n%VERSION%`n1`n`n%LICENSE%`nMIT`n`n%BASE%`nfixture`n`n%URL%`nhttps://example.invalid`n`n%DESC%`nFixture runtime`n")
    [IO.File]::WriteAllText((Join-Path $pacman 'files'), "%FILES%`nucrt64/bin/fixture.dll`n")
    $parsed = @(Read-PrismMsysPackages $fixture)
    $serialized = $parsed | ConvertTo-Json -Depth 3 -Compress
    if ($parsed.Count -ne 1 -or $serialized.Length -gt 1024 -or $serialized -match 'PSDrive|PSProvider') { throw 'Package metadata must serialize as plain values' }
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
    New-Item -ItemType Directory -Force (Join-Path $fixture 'payload\tiff-runtime') | Out-Null
    Write-PrismJson (Join-Path $fixture 'payload\tiff-runtime\manifest.json') @{ schemaVersion = 1; version = '4.7.2'; jbig = $false; sourceSha256 = 'untrusted' }
    $blocked = $false
    try { Assert-PrismTiffProvenance (Join-Path $fixture 'payload') | Out-Null } catch { $blocked = $true }
    if (-not $blocked) { throw 'Untrusted TIFF provenance was accepted' }
    $licenseRoot = Join-Path $fixture 'toolchain'
    $licenseRelative = 'share/licenses/fixture/LICENSE'
    $licensePath = Join-Path $licenseRoot $licenseRelative
    New-Item -ItemType Directory -Force (Split-Path -Parent $licensePath) | Out-Null
    [IO.File]::WriteAllText($licensePath, 'reviewed permissive license')
    $package = [pscustomobject]@{ name = 'fixture'; version = '1'; files = @($licenseRelative) }
    $runtime = @([pscustomobject]@{ package = 'fixture'; path = 'fixture.dll' })
    $review = [pscustomobject]@{ runtimeReviews = @([pscustomobject]@{
        package = 'fixture'; version = '1'; runtimeFiles = @('fixture.dll')
        licenseFiles = @([pscustomobject]@{ path = $licenseRelative; sha256 = Get-PrismHash $licensePath })
    }) }
    if (-not (Find-PrismRuntimeReview $package $runtime $licenseRoot $review)) { throw 'Matching component review was rejected' }
    $package.version = '2'
    if (Find-PrismRuntimeReview $package $runtime $licenseRoot $review) { throw 'Changed component version was accepted' }
    $package.version = '1'
    $runtime[0].path = 'unreviewed.dll'
    if (Find-PrismRuntimeReview $package $runtime $licenseRoot $review) { throw 'Unreviewed component DLL was accepted' }
    $runtime[0].path = 'fixture.dll'
    [IO.File]::WriteAllText($licensePath, 'changed license')
    if (Find-PrismRuntimeReview $package $runtime $licenseRoot $review) { throw 'Changed license bytes were accepted' }
    $blocked = $false
    try { Assert-PrismDistributionLicenses @{ packages = @(@{ name = 'fixture'; version = '1'; review = 'reviewed-LGPL-source-required'; sourcePackage = 'missing.src.tar.zst' }); unmatchedDlls = @() } } catch { $blocked = $true }
    if (-not $blocked) { throw 'LGPL runtime without corresponding sources was accepted' }
    Write-Output 'PASS: payload/ZIP integrity, GPL and TIFF gates, exact component review, missing LGPL source rejection'
}

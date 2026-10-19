# Package only the exact tested Release deployment. License gates apply to distribution.
param([string]$MsysRoot = 'C:\msys64')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'package-validation.ps1')
$root = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$deploy = Join-Path $root 'deploy'
$release = Join-Path $root 'build-release\prism.exe'
$manifestPath = Join-Path $deploy 'candidate-manifest.json'
if (-not (Test-Path -LiteralPath $manifestPath)) { throw 'Tested deployment manifest missing; run tools/deploy.ps1 after the Release build passes' }
$tested = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$identity = Get-PrismIdentity $root
if ($tested.version -ne $identity.version -or $tested.gitCommit -ne $identity.gitCommit -or $tested.sourceSha256 -ne $identity.sourceSha256) { throw 'Source identity differs from tested deployment; rebuild and redeploy before packaging' }
Assert-PrismPayload $deploy $tested $release
$dirtySuffix = if ($identity.gitDirty) { '-dirty' } else { '' }
$name = "Prism-$($identity.version)-$($identity.gitCommit.Substring(0, 12))$dirtySuffix-$($identity.sourceSha256.Substring(0, 12))"
$dist = Join-Path $root "dist\$name"
$stage = Join-Path $dist 'stage'
# Never delete an existing candidate; a failed/repeated run receives a new directory.
if (Test-Path -LiteralPath $dist) { $dist += '-' + [DateTime]::UtcNow.ToString('yyyyMMddHHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 6); $stage = Join-Path $dist 'stage' }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item -Path (Join-Path $deploy '*') -Destination $stage -Recurse -Force
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination (Join-Path $stage 'LICENSE.txt')
$notes = @'
PRISM PRERELEASE - TESTER GUIDE
==============================
Candidate identity is in candidate-manifest.json. Include that file with feedback.
This is the Windows package of the Prism drawing-app prerelease.
Prism's original code uses MIT; see LICENSE.txt. Third-party terms remain separate.

INSTALL / PORTABLE / USER DATA
Windows 10/11 64-bit. Installer: run the versioned Prism-Setup executable.
Portable: extract the versioned ZIP to its own folder and run prism.exe.
Qt and its runtime libraries are supplied as separate replaceable DLLs.
Uninstall removes installed application files; it preserves user projects,
settings, brushes, recovery snapshots and recordings. Save artwork outside the
installation directory. See THIRD-PARTY for runtime licenses and provenance.
This candidate is unsigned. Verify its SHA-256 against artifacts-manifest.json;
the hashes detect changed files and do not establish the publisher's identity.

DRAWING WORKSPACE
Every entry starts with the full interface. Layers is above the compact Colour
shelf. Brush Lab and the full Colour picker open as panels; Colour can be pinned.
The four-corner toolbar icon or Tab enters focus mode. The workspace-layout icon
at the top right or Tab restores the full interface. During a stroke the restore
icon disappears, then returns. There are no edge-hover panels.
Q opens Quick Controls near the pen. Ctrl+1: Layers; Ctrl+2: Brush Lab;
Ctrl+3: Colour. Use recent colours and pinned/recent brushes. Tool shortcuts can
be configured. F1 opens offline help, including current shortcut assignments.
Text fields keep their editing keys. Try both light and dark themes.

SAVE / OPEN / RECOVERY
Ctrl+S saves the native layered .prism project; Ctrl+Shift+S opens Save As.
Flat PNG/JPEG/WebP exports are separate from saving editable artwork.
Check confirmation and persistent errors. Cancel and retry failed operations.
Unsaved work prompts Save/Discard/Cancel on document replacement or exit.
Autosave keeps separate recovery snapshots; Start Page offers Recover Artwork.
Always retain a separate copy of valuable work while testing a beta candidate.

DRAWING RECORD / STORAGE
Recording starts automatically per document and cannot be paused or disabled.
REC is status, not a toggle. .prismrec retains every recorded frame; GIF/MP4
sample the timeline to a bounded timelapse, retaining its first and final frame.
An export records the app's drawing sequence, not the identity of its artist.
Watch for recording errors/gaps; retry must retain the valid earlier sequence.
Normal export requires the final frame. Explicit partial exports remain labeled.
Storage lists session status and size, offers export/open-folder and confirmed
deletion of inactive sessions. Nothing is automatically deleted by retention.
Manual deletion is permanent; export or back up a session before deleting it.
Artwork recovery and recording recovery are distinct actions on the Start Page.

TEST PRIORITIES
1. Pixel/inch/mm dimensions and 72/300 DPI; clipboard pixels and transparency.
2. HB/2B graphite, Ink release taper, Paint, eraser, quick light/heavy strokes.
3. Undo/redo; groups, clipping/alpha lock; reorder layers and drag opacity.
4. Selections, crop/resize, shapes, transforms, fills and filters.
5. Save/reopen .prism: layers, dimensions/DPI, clipping and pixels must survive.
6. Cancel/discard/save during New/Open/close; failed saves and recovery retries.
7. Q/Tab/Ctrl+1/2/3, F1, pen-sized controls, pinned Colour and light/dark themes.
8. Continuous edits with autosave, disk/storage failures, recording retry and
   final-frame/partial exports. Check GIF/MP4 orientation with an external decoder.
9. 1000x620 and 1280x780 logical windows at Windows 100/125/150/200% scaling.
10. Install/upgrade/uninstall on clean Windows; verify user-data preservation.

ACCEPTANCE STILL REQUIRES EVIDENCE
Automated core/selftests and synthetic interaction checks do not establish pen
feel, GPU-driver reliability or installer correctness on a clean computer.
Physical Wacom/Huion/XP-Pen validation, a completed illustration, eight-hour
drawing/recording/recovery/export, and clean-machine install/upgrade/uninstall
remain external acceptance gates unless separately recorded for THIS identity.
Linux/macOS CI checks build/core tests only; it does not certify those releases.
Current colour support: RGB/Alpha, 8-bit, sRGB. No wet pigment simulation.
TIFF supports the configured non-JBIG codecs. TIFF with JBIG compression is
unsupported; the optional GPL JBIG-KIT runtime is not bundled.
Manifest checks identify exactly which automated gates ran; unlisted checks
are unverified. License provenance is not a claim of legal compliance.

REPORT A PROBLEM
Attach candidate-manifest.json, steps, expected/actual result and screenshots.
Include Windows version/scale, GPU, tablet model/driver, canvas size/layer count,
and %APPDATA%\Prism\Prism\prism.log. Do not send private artwork unnecessarily.
'@
[IO.File]::WriteAllText((Join-Path $stage 'README-TESTER.txt'), $notes, [Text.UTF8Encoding]::new($false))
$notices = Join-Path $stage 'THIRD-PARTY'
$inventory = Export-PrismNotices $stage $notices $MsysRoot
$sources = Join-Path $notices 'SOURCES'
Copy-PrismCorrespondingSources $inventory $sources
Copy-Item -LiteralPath (Join-Path $root 'third_party\LIBRARY-REPLACEMENT.txt') -Destination $notices
Copy-Item -LiteralPath (Join-Path $root 'third_party\NOTICES.txt') -Destination $notices
Assert-PrismDistributionLicenses $inventory $sources
$licenseStatus = 'byte-matched component review with corresponding sources and replacement instructions; not a legal certification'
Copy-Item -LiteralPath (Join-Path $root 'third_party\README.md') -Destination (Join-Path $notices 'README.md')
# Installer removes only these exact relative application files, never user-data trees.
$ownedDirectories = @(Get-ChildItem -LiteralPath $stage -Directory -Recurse | Sort-Object { $_.FullName.Length } -Descending | ForEach-Object { $_.FullName.Substring($stage.Length + 1) })
[IO.File]::WriteAllLines((Join-Path $stage 'installed-directories.txt'), $ownedDirectories, [Text.Encoding]::Unicode)
$ownedFiles = @((Get-PrismPayload $stage).path) + @('candidate-manifest.json', 'installed-files.txt', 'installed-directories.txt')
[IO.File]::WriteAllLines((Join-Path $stage 'installed-files.txt'), ($ownedFiles | Sort-Object -Unique | ForEach-Object { $_.Replace('/', '\') }), [Text.Encoding]::Unicode)
$manifest = [ordered]@{ schemaVersion = 1; version = $identity.version; gitCommit = $identity.gitCommit; gitDirty = $identity.gitDirty; sourceSha256 = $identity.sourceSha256; releaseExeSha256 = $tested.releaseExeSha256; testedUtc = $tested.testedUtc; checks = $tested.checks; acceptance = $tested.acceptance; distributionLicenseStatus = $licenseStatus; installerPayloadVerification = 'Exact staged hashes checked before/after NSIS; clean-machine installation NOT verified'; payload = Get-PrismPayload $stage }
Write-PrismJson (Join-Path $stage 'candidate-manifest.json') $manifest
Assert-PrismPayload $stage $manifest $release
# Stage remains reviewable if this fails, but no distributable ZIP/installer is emitted.
Assert-PrismDistributionLicenses $inventory $sources
$makensis = Join-Path $MsysRoot 'mingw64\bin\makensis.exe'
if (-not (Test-Path -LiteralPath $makensis)) { throw "NSIS compiler missing: $makensis. Install the build tool explicitly before packaging." }
$setup = Join-Path $dist "$name-Setup.exe"
& $makensis '-V2' "-DSTAGE=$stage" "-DOUTPUT=$setup" "-DCANDIDATE_VERSION=$($identity.version)" (Join-Path $PSScriptRoot 'installer\prism.nsi') | Write-Output
if ($LASTEXITCODE -ne 0) { throw "NSIS failed: $LASTEXITCODE" }
Assert-PrismPayload $stage $manifest $release
$zip = Join-Path $dist "$name-win64.zip"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
Assert-PrismZip $zip $manifest
$after = Get-PrismIdentity $root
if ($identity.sourceSha256 -ne $after.sourceSha256 -or $identity.gitCommit -ne $after.gitCommit) { throw 'Source changed during packaging; these artifacts are not a verified candidate' }
Write-PrismJson (Join-Path $dist 'artifacts-manifest.json') ([ordered]@{ candidate = $manifest; artifacts = @([ordered]@{ path = [IO.Path]::GetFileName($setup); sha256 = Get-PrismHash $setup }, [ordered]@{ path = [IO.Path]::GetFileName($zip); sha256 = Get-PrismHash $zip }); verified = @('staged installer input before/after NSIS', 'all ZIP entries including prism.exe'); cleanWindowsInstallerAcceptance = 'pending' })
Write-Output "Verified candidate: $dist"

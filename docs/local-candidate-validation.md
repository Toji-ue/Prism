# Local candidate validation

## Recording timelapse candidate — 2026-10-17

Candidate **0.3.0-beta.1**, Windows/MSYS2 UCRT64, Qt 6.11.1, Release.
This section supersedes the artist workflow candidate below for recording.
`deploy/candidate-manifest.json` remains authoritative for the deployed commit,
source fingerprint, executable and runtime payload. Local deployment is separate
from distribution approval.

### Implementation and automated proof

- Recording defaults to 1.5 seconds and 1920 px long edge. The separate 320 px
  Recording dock is closed on document entry and hidden in Focus. The preview
  loads one stored-frame snapshot on opening, with explicit Refresh/scrubbing;
  it does not repeatedly rescan a long session as drawing continues.
- Local timelines use canonical paths and saved-file fingerprints. Save As
  freezes the parent before later pen events and starts a child referencing
  shared immutable sources. Link failures cannot report success. Raw deletion
  and export destinations cannot bypass shared-source/internal-storage guards.
  A damaged local index blocks raw source deletion.
- Portable v3 bundles retain original v1/v2 streams and validate hashes, lengths,
  references and limits before staged publication. Cancel/failure rolls back
  imports and preserves existing export destinations. Gap/unknown-completion
  sources require explicit Partial export. Project and brush formats are unchanged.
- Full Release CTest: **152/152 passed**. Isolated Release `--selftest`: **zero
  failures**. The recovery fixture now exports outside protected Storage before
  staging a recovery source; async export waits for the preceding link probe.
- Native Windows QTest: **73 UX assertions, zero failures**, plus the Focus
  regression. Recording assertions cover real REC input, dock width, initial
  preview, numeric capture interval, keyboard access to Export, Focus capture,
  zoom and viewed-canvas-centre preservation on opening/closing the dock.
- Independent external ffmpeg/Pillow comparison passed for configured MP4/GIF,
  sampling all 12 frames of asymmetric source fixtures, including first/last.
  Upright error was lower than vertical-flip error. This exercises the independent
  decoder rather than relying only on Prism's Media Foundation readback.
- A 2,000-frame, 256x256 streaming test passed, including first/last pixels,
  complete frame count, gap state and hash verification. Sampled process working
  set peaked at **27.15 MiB** (72 samples), versus **500 MiB** for retaining all
  decoded frames. Metadata/index memory can grow with frame count; decoded-frame
  storage does not. This short synthetic test is not the eight-hour soak.
- An 8500x8000 canvas records at 720 and Original without a false queue-limit
  rejection. One oversized immutable snapshot is allowed alone. Original decode
  uses bounded preallocation instead of changing Qt's global allocation policy.
  The cadence timer avoids per-point recorder mutex/clock polling; held strokes
  remain recordable and idle creates no extra frames, covered by a regression.
  PNG compression is explicitly set on Qt's 0–100 scale; existing source hashes
  are preserved.

Evidence: `build/record-final-tests3.log`, `build/record-selftest-final3/`,
`build/record-native-final3/`, `build/record-media-evidence-v2/`,
`build/record-orientation2.log`, `build/record-memory-test.log` and
`build/record-memory-samples.json`. Deployment repeats Release tests and staged
selftest/formats checks; its manifest supplies the final executable identity.

The final local Release executable SHA-256 is
`B09244CB6B6F2C981967BD74A79E32C54019E30096ABC2AC10868CA79D445D35`.
An unrelated untracked `dsh-session-session-*` directory is preserved. It can
make the manifest's repository-wide `gitDirty` true; the manifest separately
identifies the application source fingerprint and committed revision.

### Performance gate — open

Three interleaved before/after runs used visible Windows rendering, 300 points,
and default capture settings (new capture at 1920 px). Before is the previous
deployed candidate (`28C75D0F…`); after is `09C946B6…`, before the final opt-in
diagnostics/logging mutex. Values below are medians of the three run statistics.

| Canvas | Median point before → after | Change | Mouse-up before → after | Texture upload before → after |
| --- | --- | --- | --- | --- |
| 1280×800 | 0.0272 → 0.0272 ms | 0% | 3 → 3 ms | 14,745,600 → 12,910,592 bytes |
| 2000×3000 | 0.0201 → 0.0136 ms | −32.3% | 12 → 9 ms | 59,291,136 → 47,232,512 bytes |
| 4000×4000 | 0.0281 → 0.0328 ms | **+16.7%** | 22 → 23 ms | 146,477,056 → 157,487,104 bytes |

The 4000×4000 result exceeds the agreed 10% median threshold. **Recording
performance acceptance is not complete.** Earlier paired batches also varied;
selecting a faster individual run would not close this gate. Evidence:
`build/record-final-paired-*` and `build/record-final-bench-summary.json`.

Capture cadence no longer polls the recorder mutex/clock at every point; an
already-pending timer captures current pixels. The existing PNG worker uses low
priority. A diagnostic run on the final binary (`build/record-phase-profile2/`)
measured a 119 ms instrumented resize phase (including its initial log write)
and 157 ms PNG encoding for a 4000×4000 snapshot; both
finished 889 ms before the CPU point loop began. This does **not** establish
encoding contention as the cause of the median regression. Diagnostics are
opt-in via `PRISM_RECORD_PROFILE=1`; regular runs do not log worker timings.
The shared log sink is synchronized for GUI/worker diagnostics.

Further performance work must reproduce and isolate the regression before
claiming acceptance; the local deployment is available for development only.

### Visual coverage

Two batches inspect Recording dock, shared export dialog and Focus in light/dark
at requested logical sizes 1280x780 and 1000x620: **12 captures per batch**.
The first batch found the unthemed dialog header and a race leaving initial
preview empty; both were fixed and the confirmation batch plus native QTest
passed. Controls below the minimum-height viewport scroll and keyboard focus
reveals Export. The dock does not create a second Layers/Colour panel.

Artifacts: `build/record-visual-*`, `build/record-confirm-*` and
`build/record-confirm-contact-{light,dark}.jpg`. Process scale is 1 and the Windows
host DPR is 1.25; this does not establish other Windows scaling settings or
physical pen accessibility. No QML reference/type/binding-loop errors appeared
in accepted captures.

### Review and remaining gates

Separate Standards/Spec reviews found shared-source deletion, retry lifecycle,
parser bounds, import rollback/thread ownership, stale preview/recovery, final
capture, live metadata, large canvas, destination aliases, Save As and failed-link
issues. Fixes have regression coverage; focused final static rechecks reported
no remaining findings in those items. The initial-preview follow-up also had no
obvious state loop.

The 4000×4000 performance gate, physical tablet, actual eight-hour
drawing/recovery/export, clean Windows,
remote Windows/macOS/Linux CI and supported-hardware latency remain pending.
The TIFF/libtiff/GPL jbigkit chain and other runtime licenses are not approved;
external packaging remains blocked. No UX 40/40 or production-ready claim is made.

## Previous artist workflow candidate — 2026-10-16

Candidate **0.3.0-beta.1**, Windows/MSYS2 UCRT64, Qt 6.11.1, Release.
This section supersedes the earlier candidate below. Local implementation and
automated verification are complete; hardware and release gates remain open.

### Identity and correctness

- Release executable SHA-256:
  `28C75D0FFC5A414F4ACB2FBF9B07EB604CDDD98A6C82C6B66D96E491C3619D2C`.
  `deploy/candidate-manifest.json` is authoritative for the deployed commit,
  executable, source fingerprint and complete runtime payload. Deployment must
  pass `tools/deploy.ps1`; a successful source build alone is insufficient.
- Full Release CTest: **135/135 passed**. Release `--selftest`: **zero failures**,
  with isolated settings/recovery and writable TEMP/TMP. The selftest explicitly
  selects a paint preset after deleting its active user preset, because deletion
  now correctly applies the replacement preset, which can be an eraser.
- Native Windows `--demo-ux-beta-test`: **63 assertions, zero failures**.
  The Focus regression also passed. Real QTest input covers Choose/Edit, long
  preset-list navigation and pinning, search, Reset/Save copy/delete confirmation,
  HSV/Hex validation and Escape, numeric Resize Canvas and layer Opacity with
  undo, Q/panel exclusivity, text-input shortcuts, notices and Start Page actions.
  Focused controls scroll into view without moving the canvas.
- Mouse slider drags, slider keyboard nudges, layer reordering and Q placement at
  all four content corners passed. Removing a missing Recent entry preserves
  actual files. Import, selection and Reset retain the artist's colour; preset
  deletion retains identity or applies its replacement settings.
- Evidence: `build/ux-keyboard-accepted-v4/prism.log`,
  `build/ux-final-selftest-green/`, `build/ux-controls-*`,
  `build-release/Testing/Temporary/` and `build/ux-fixtures/`.

### Visual coverage and limits

Two visual passes inspected **64 captures** across light/dark, requested logical
window sizes 1280×780 and 1000×620: Start Page, full workspace, Choose, Edit,
Colour, Focus and Q at process factor 1; Start/full/Focus additionally at factors
1.25, 1.5 and 2. The Windows host DPR was 1.25, so these process factors are not
evidence of changing Windows display settings to 100/125/150/200%. At high
factors, the host display can constrain physical window dimensions.

The matrix used executable `A59FE102C2E8DD3C334FAE7152B5D2D454A1A059E230ACAEAFB222BCFA0DDA1F`.
After adding numeric layer Opacity, four focused light/dark workspace captures
at both sizes and the 63-assertion interaction suite verified the final binary.
Long preset names/lists, pinned brushes, missing recents and simultaneous artwork
and recording recovery have separate fixtures. No QML reference/type/binding-loop
errors were observed in accepted captures. This is not exhaustive accessibility
or physical pen acceptance.

Artifacts: `build/ux-final-*`, `build/ux-contact-*.png`,
`build/ux-opacity-*/screen.png` and `build/ux-fixtures/`.

### Rendered 300-point comparison

Both versions drove real mouse events in visible rendering Windows windows on
the same machine, with isolated settings and the default Solid preset. Baseline
is commit `e0db7467a755e3ea717823f17adc3ea8952b0f11`; after is the final hash above.
These are CPU processing measurements, not physical pen-to-pixel latency.

| Canvas | Average move, before → after | Median point, before → after | Mouse-up, before → after | Uploaded bytes, before → after |
| --- | --- | --- | --- | --- |
| 1280×800 | 0.0600 → 0.0567 ms | 0.0256 → 0.0245 ms | 2 → 2 ms | 12,386,304 → 11,337,728 |
| 2000×3000 | 0.2767 → 0.1200 ms | 0.0322 → 0.0122 ms | 22 → 8 ms | 65,320,448 → 41,989,632 |
| 4000×4000 | 0.4067 → 0.3300 ms | 0.0532 → 0.0266 ms | 27 → 20 ms | 165,351,424 → 138,088,448 |

After runs rendered 9/11/20 frames and uploaded 52/169/543 tiles respectively.
No median regression above 10% appeared in this local comparison. Single-run
timing and frame scheduling vary; this does not establish an engine speedup.
Raw evidence: `build/ux-flow-before-*` and `build/ux-flow-after-*`.

### Independent review and remaining acceptance

- **Standards:** separate review and focused rechecks found Resize Canvas numeric
  entry, Tab navigation and focus-scroll/list navigation gaps. They are fixed,
  covered by native interaction tests; the final recheck had no open findings.
- **Spec:** separate review found imported presets not applying, thumbnail
  identity, preset deletion identity and colour retention issues. They are fixed
  with regression coverage; the final focused recheck had no open findings.

Physical tablet/pen display and at least three artists completing
Sketch → Ink → Paint → Save/export remain pending. Record device/driver/tilt,
wrong-panel searches, misclicks and UI obstruction before UX acceptance. The
eight-hour recording, clean Windows installation, remote Windows/macOS/Linux CI,
supported-hardware latency and distribution-license gates remain open. The TIFF
dependency chain and other runtime licenses have not been approved or removed.
No UX 40/40 or production-ready claim follows from these automated results.

## Previous candidate — 2026-10-14

Candidate: **0.3.0-beta.1**, Windows/MSYS2 UCRT64, Qt 6.11.1, Release.
This records local implementation and automated evidence. It does not establish
UX 40/40, production readiness or permission to distribute the runtime.

## Candidate identity

- Release executable SHA-256 after the panel routing fix: `56D0FD65A554C7D7BBE8C0FD4AC386A92729CCF21AB1652F68FACE3C48965270`.
- `PrismVersion.txt` supplies the compiled and packaging version. A root `VERSION`
  file must not be used: on Windows it shadows the C++ `<version>` header.
- `tools/deploy.ps1` rebuilds Release, reruns the suite, stages fresh dynamic Qt
  and codec dependencies, probes the compiled version, formats and selftest with
  a Windows-only PATH, then verifies the installed payload against its manifest.
  MSYS2's QML scanner directory is obtained from `qtpaths6` and added to PATH;
  the installed scanner is not in the regular UCRT64 binary directory.
  Plugin destinations match `qt.conf`; the stage includes `qoffscreen.dll` for
  the isolated selftest as well as `qwindows.dll` for normal drawing.
- The authoritative deployment result is `deploy/candidate-manifest.json`: its
  source fingerprint, commit, executable hash and payload hashes identify the
  files actually tested. An unrelated untracked `dsh-session-*` directory can
  leave `gitDirty` true; it is excluded from the candidate source fingerprint.
- Previous deployment files are retained under `build/deploy-previous-*` rather
  than deleted. Earlier `release-validation.md` describes an older candidate.

## Correctness and interaction evidence

- Full Release CTest suite: **130/130 passed**. Deployment repeats the complete
  suite against the candidate; its result is a required deployment gate.
- The final recorder/artwork focused rerun passed **19/19** after completion
  metadata and complete-export guards were corrected.
- Release `--selftest`: zero failures in local validation. Deployment repeats it
  using only the staged runtime, without MSYS2 on PATH.
- Native `--demo-ux-beta-test`: zero failures. Real QTest input exercises New
  Document units/DPI/layers, cancel, an active pointer grab through a document
  transition and release, help, Colour pinning and Start Page errors.
- Native focus interaction checks cover Tab/text input, Q, Ctrl+2, keyboard/mouse
  restore, stroke suppression, and preservation of zoom and viewed canvas centre.
- Light/dark Start Page, full workspace and focus captures were generated at
  1280×780 and 1000×620; minimum workspace/focus captures also cover Windows
  scaling 100/125/150/200%. The inspected light minimum workspace and dark minimum
  focus views retained readable controls, square artwork and the restore button.
  These checks do not establish pen reachability or exhaustive accessibility.
- Local evidence directories: `build/final-ui-*`, `build/final-scale-*` and
  `build-release/Testing/Temporary`. Fixtures isolate settings/recovery and use
  writable `build/test-temp` for TEMP/TMP.
- Packaging validation fixtures passed payload/ZIP tamper checks and the
  fail-closed license gate. No distribution approval follows from this result.

## Resolved continuation defects

- Retry and archive scans reuse the existing recorder I/O worker and publish an
  immutable index through queued GUI callbacks. Busy state prevents duplicate
  retries and session replacement while repairing. GUI waits only for the
  already-exited worker before joining; spool scanning runs off the GUI thread.
- Document transitions preserve the active canvas grab through pen release and
  commit/cancel. Final-frame acknowledgments match both spool identity and
  capture ID. Save results acknowledge only the captured history revision.
- Discard removes artwork recovery only after close is accepted. Cancelling a
  recording-error close retains the current artwork and its recovery snapshots.
- Recovered spool bytes and sidecar metadata remain immutable. A valid prefix
  without completed metadata still requires explicitly partial export. Completed
  sessions appear in Storage rather than crash recovery; no automatic deletion.
- Completion metadata is written only after queued frames drain. Both export
  entry points reject incomplete coverage and active edits for a normal export;
  the final capture must be acknowledged before complete success is reported.

## Independent code review

Standards and Spec reviewed the working changes against
`5d5e0edda884f8bd065c8689bc4d92b729284ade` in separate agents.

- **Standards:** fixed premature recovery deletion on cancelled close and the
  synchronous UI archive scan. Focused recheck reported both resolved.
- **Spec:** fixed clean crash prefixes being treated as complete and completed
  recordings appearing as crash recovery. Focused recheck reported both resolved.

The findings have regression coverage. Review is a local code check, not a
substitute for the hardware, soak, installer or remote CI gates below.

## Rendered 300-point benchmark

The continuation baseline executable (`AD1E2A0280B2B0ECACE1D0549C5D052989C52E4D37F13BB04B274FAF8C76E2B9`) drove real QTest mouse moves in visible, rendering
Windows windows, with the default Solid preset. Each run used isolated settings.
Values below are CPU processing, not measured physical pen-to-pixel latency.

- **1280×800:** average move 0.0600 ms, median point 0.0253 ms, mouse-up 1 ms;
  10 rendered frames, 56 uploads, 12,386,304 uploaded bytes.
- **2000×3000:** average move 0.1467 ms, median point 0.0233 ms, mouse-up 9 ms;
  13 rendered frames, 183 uploads, 45,659,648 uploaded bytes.
- **4000×4000:** average move 0.3767 ms, median point 0.0328 ms, mouse-up 24 ms;
  28 rendered frames, 583 uploads, 148,574,208 uploaded bytes.

Raw logs/screenshots: `build/candidate-bench-<width>x<height>/`. Hidden-window
runs with zero rendered frames/uploads were rejected as performance evidence.

A separate, sequential three-pair 4000×4000 comparison measured the former
deployment against the recording-capture throttle optimization. Medians across
the three runs changed: average move **0.5000→0.4667 ms**, median point
**0.0625→0.0464 ms**, mouse-up **31→27 ms**, uploaded bytes
**148,049,920→153,292,800** (+3.54%). One new-run mouse-up was 73 ms; this small
sample has variability. Logs are under `build/clean-visible-*`. This comparison
used the intermediate optimized build; the continuation baseline results are above.
No >10% median regression was observed in this comparison. Graphite tablet feel
and long-session responsiveness still require hardware/soak evidence.

## Panel routing follow-up — 2026-10-14

- Full workspace keeps docked Layers and the Colour shelf. Floating tabs offer
  Brush Lab and unpinned Colour only. Ctrl+1 closes the popup and focuses the
  selected docked layer row; Ctrl+3, the Window menu, toolbar swatch and More
  share one Colour path or focus the pinned picker. Focus mode keeps all three
  floating tabs, with unchanged IDs 0/1/2. A single-tab popup hides the tab strip.
- Pinning Colour transfers focus to the dock. Start Page, document replacement,
  mode changes, Escape, outside clicks and strokes close the floating panel.
  Popup bounds include the menu/toolbar offset and fit the minimum window.
  Workspace popups use an opaque shared panel surface so docked text/icons do
  not show through; Focus mode retains its glass surface.
- The expanded native `--demo-ux-beta-test` passed **31 assertions, zero failures**
  on the final hash above at 1000×620. An earlier routing build also passed at
  light 1280×780. Real QTest mouse and
  keyboard input covers the Window Colour menu, toolbar/More, filtered tabs,
  keyboard tab activation, pinned picker, text-field exclusions, stroke blocking,
  focus transfer, popup bounds and document/Start Page closure. Logs are in
  `build/panel-shipped-interaction` and `build/panel-accepted-*`; the unchanged
  CTest suite remains 130 tests. The Focus regression also checks Q, Tab, restore,
  stroke suppression and preservation of zoom and viewed canvas centre.
- Final screenshot fixtures use `build/panel-shipped-*`: light/dark, 1280×780 and
  1000×620, full Brush Lab/full Colour/Focus Colour. Open-panel fixtures do not
  draw a later demo stroke, which would correctly close the popup. Hidden-window
  runs without layout polishing were rejected as tab-click evidence.
- F1 Help and AGENTS workspace guidance describe the corrected routing. CI uses
  a 12-second screenshot delay so the longer interaction run can finish. Remote
  CI and real pen input remain unverified; this fix adds no format, brush or
  rendering changes. Benchmark measurements above describe the earlier hash.

## Open release gates

- Physical pen display/tablet: model, driver, pressure/tilt, completed illustration,
  HB/2B/Ink touch/lift/overlap, fast/slow drawing and shading impressions.
- Eight-hour actual drawing session, retry responsiveness, forced crash, recovered
  recording and full/partial exports; keep user recordings until confirmed deletion.
- Clean Windows install/upgrade/uninstall and user-data preservation.
- Supported-hardware performance, including the recorded 4000×4000 regression.
- Repeat exact package/source/archive review whenever a distributed runtime
  changes. The identities below passed this gate; changed or unknown runtimes
  still require review. See `third_party/release-license-review.md` and
  `third_party/native-release-license-review.md`.

These gates remain open in `roadmap.md` and `ux-beta-plan.md`.

## Three-platform prerelease continuation — 2026-10-19

This section supersedes earlier statements that all remote CI or Windows
distribution work is unverified. Run **39097262425** passed Windows, macOS 14
arm64 and Ubuntu 24.04 x86_64 build/tests. Each platform passed **152 tests**;
Windows also passed selftest and native QTest interactions. Final run
[39199227884](https://github.com/Toji-ue/Prism/actions/runs/39199227884)
at commit **93e470551b08c302db4cfd39043f616474e67789** passed all three jobs,
including native package/source/archive validation and startup binding checks.

### Windows candidate

- Source commit: `8deb20e0cc47f91e7fe185bb8804b2b414555331`.
- Source fingerprint: `2afe57ce72966733925d15ad5a14bb8f0070da48302c55d1f93be012d61fea6e`.
- Executable SHA-256: `f00b9d07e10149818a99dd71506fa109e54987ce081eff58e30f586f380a8d5a`.
- Canonical deploy passed **152/152**, version/format probes and staged selftest.
  The visible-window UX run without a loose Prism module passed **0 failures**
  (`build/embedded-theme-visible-ux`). A hidden-window attempt failed seven
  popup/grab assertions and is excluded from acceptance evidence.
- `tools/package.ps1` produced and verified installer/ZIP in
  `dist/Prism-0.3.0-beta.1-8deb20e0cc47-2afe57ce7296`.
- Installer SHA-256: `d85397adc08992646bcc3b4d80ee8697fa7c102a1af1f578504f8681bd7731b0`.
- ZIP SHA-256: `d376fc07af0f6388c9fc5334288ed13aa16b8cc4f5a3e2b35dd5ab6a50b52a8a`.
- Published GitHub assets returned the same SHA-256 digests.

### Published native candidates

- macOS 14+, Apple Silicon: `Prism-0.3.0-beta.1-macos-arm64-93e470551b08.dmg`,
  SHA-256 `0a9220f5680051a1c1d6cbcda01b2e160d95074cc9442a6aba6e07f1c3421dbf`.
  The alternative `.tar.gz` has SHA-256
  `a032d5f5bc2d264c8868b488860cc3bef22a0cf1a3d59aacc6aa149d8a862495`.
- Ubuntu 24.04 x86_64/glibc 2.39:
  `Prism-0.3.0-beta.1-linux-x86_64-93e470551b08.tar.gz`, SHA-256
  `e89e5664208c8c01bfdca140db1de9489631af7b3e79d8eef53f7d9f9abf5259`.
- Native packages use commit `93e4705`; the only change after the Windows
  source `8deb20e` is ignoring the generated CI `aqtinstall.log`.
- [Published prerelease v0.3.0-beta.1](https://github.com/Toji-ue/Prism/releases/tag/v0.3.0-beta.1)
  contains nine verified assets: five binaries/archives, three platform manifests
  and `SHA256SUMS.txt`. GitHub upload digests match the local artifact hashes.
  The release tag targets the tested native commit above; later evidence-only
  documentation commits do not change these artifact identities.

### Dependency and native fixes

TIFF is retained. The approved Windows build uses libtiff 4.7.2 without JBIG;
its upstream suite passed **18/18** and Qt TIFF pixel/alpha checks matched the
old runtime baseline. The two GCC runtime DLLs have the owner's explicit
2026-10-18 exception sign-off. Six signed complete MSYS2 source packages,
notices and library-replacement instructions accompany the Windows artifact.

Native inventories review exact runtime paths and SDK SPDX hashes. macOS uses
replaceable Qt frameworks with an ad-hoc signature; it is not Developer ID
signed/notarized. Linux retains reviewed ICU 73.2 and builds its TIFF plugin
against the approved no-JBIG runtime: the SDK plugin required libtiff.so.5.
Archive gates check source hashes, payload bytes, links and permissions.

Visual review caught a missing embedded QML import even though startup returned
zero. `qrc:/` is now registered for the explicit `qrc:/Prism` module contract.
An isolated Windows fixture reproduced the failure and then passed after the
fix. Native Start Page/workspace startup rejects binding errors. The generated
`aqtinstall.log` is ignored explicitly; the clean-source gate remains enforced.

Physical tablet/three-artist UX, eight-hour recording, clean installation and
the performance gate remain open. MP4 encoding remains Windows-only. These
artifacts are published prereleases and do not establish UX 40/40 or production
readiness.

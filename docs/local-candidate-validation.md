# Local candidate validation

## Artist workflow candidate — 2026-10-16

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
- Completed remote Windows CI, macOS/Linux build/test runs and supported hardware
  performance; workflow files alone are not successful CI runs.
- Distribution license review remains blocked by the shipped TIFF chain
  `qtiff.dll → libtiff-6.dll → libjbig-0.dll` (jbigkit GPL-2.0), plus other runtime
  review items in `third_party/README.md`. No codec was removed and no dependency
  was approved by this task. Local deployment and external packaging are separate.

These gates remain open in `roadmap.md` and `ux-beta-plan.md`.

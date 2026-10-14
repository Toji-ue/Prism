# Local candidate validation — 2026-10-14

Candidate: **0.3.0-beta.1**, Windows/MSYS2 UCRT64, Qt 6.11.1, Release.
This records local implementation and automated evidence. It does not establish
UX 40/40, production readiness or permission to distribute the runtime.

## Candidate identity

- Release executable SHA-256: `AD1E2A0280B2B0ECACE1D0549C5D052989C52E4D37F13BB04B274FAF8C76E2B9`.
- `PrismVersion.txt` supplies the compiled and packaging version. A root `VERSION`
  file must not be used: on Windows it shadows the C++ `<version>` header.
- `tools/deploy.ps1` rebuilds Release, reruns the suite, stages fresh dynamic Qt
  and codec dependencies, probes the compiled version, formats and selftest with
  a Windows-only PATH, then verifies the installed payload against its manifest.
  MSYS2's QML scanner directory is obtained from `qtpaths6` and added to PATH;
  the installed scanner is not in the regular UCRT64 binary directory.
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

The final executable above drove real QTest mouse moves in visible, rendering
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
used the intermediate optimized build; the final exact-binary results are above.
No >10% median regression was observed in this comparison. Graphite tablet feel
and long-session responsiveness still require hardware/soak evidence.

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

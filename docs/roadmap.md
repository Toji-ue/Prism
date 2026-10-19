# Prism release roadmap

This page is the project's **What's next** list. A gate closes only after its
evidence is recorded; a working UI control alone is not a release signal.

## Alpha — protect artwork and validate the core

- [x] Autosave modified documents and offer separate artwork and recording recovery after a crash. Automated snapshot and recovery checks passed; forced-crash QA remains below.
- [x] Round-trip `.prism` v1 and v2, including nested groups, clipping, alpha lock and crop origin.
- [x] Undo and redo layer property and structure changes and Resize Canvas in automated tests.
- [x] Verify Ink taper and legacy `.prismbrush` v1–v5 behavior in automated preview, commit and replay checks.
- [ ] Record eight-hour sessions to disk without a fixed frame buffer; verify `.prismrec` v1/v2 and streamed exports.
- [x] Measure 300-point stroke processing and upload volume at 1280×800, 2000×3000 and 4000×4000 in a rendering window. Current results and comparison are in [local candidate validation](local-candidate-validation.md); physical pen-to-pixel latency remains below.
- [ ] Try HB, 2B and Ink on physical pen tablets. Record model, driver, pressure/tilt support and touch, lift, overlap, fast strokes and shading impressions.

## Closed beta — complete the artist workflow

- [ ] Check the Start Page and workspace at normal and minimum size in light and dark themes, with mouse and pen.
- [ ] Confirm glass panels stay hidden during real pen strokes and every shortcut works outside text inputs. Automated engine-stroke suppression, Tab, Ctrl+1/2/3, Q and focus restore checks exist; edge-hover panels are removed.
- [ ] Exercise recovery after a forced crash and a missing Recent Files entry without overwriting the original.
- [ ] Confirm Resize Canvas, Canvas Information and About Prism on mouse and pen. The controls are implemented and Resize Canvas history is covered by tests.
- [ ] Run import/export and packaging checks on a clean Windows machine; keep macOS and Linux buildable.

## Production — release gate

- [ ] Complete data-loss and backward-compatibility review, including interrupted autosave and record export.
- [x] Move streamed record copy/encoding and verification off the UI thread. Worker `.prismrec` and video exports passed automated readback; eight-hour responsiveness is still an open soak test.
- [ ] Meet rendering and brush latency budgets on supported hardware; keep CI green for unit, pixel and interaction tests.
- [ ] Verify deployed binary hashes, image plugins, installer and update path; document licenses of shipped dependencies.
- [ ] Finish accessibility, keyboard navigation, contrast and tablet QA for both themes.

## Later, outside these gates

ICC-aware color management and a working Color Space Browser, Grayscale and 16-bit documents, wet pigment mixing, cloud services, and tablet operating systems remain future work. The New Document UI must only offer RGB/Alpha, 8-bit, sRGB until the corresponding engine and format support exist.

## UX-beta continuation

Implemented: independent save checkpoints, bounded autosave, guarded document transitions, real New Document DPI/clipboard, persistent notices, pinned colour, offline help, recorder error/retry/recovery and explicit partial coverage, retained recording Storage, version/payload identity and license gates. Current evidence is in [local candidate validation](local-candidate-validation.md), including completed multi-platform CI and exact component/source reviews. Physical tablet, eight-hour drawing, clean-machine installation and supported-hardware performance remain open. UX 40/40 and production readiness have not been established.

## Artist workflow UX — local implementation 2026-10-16

- [x] Separate Choose/Edit brush views, searchable All/Pinned/Recent lists,
  stable preset identity, temporary edits, Save copy/Reset and confirmed deletion.
- [x] Colour-only hue/SV/HSV/Hex controls, remembered neutral hue, shared pinned
  state and consistent Flow labels; Quick Controls links to Choose/Edit.
- [x] Shared themed controls, numeric entry and keyboard navigation, focused
  control scrolling, compact Start Page, safe recent removal and source-specific
  persistent error actions. Full/Focus panel routing remains exclusive.
- [x] Local Release 135-test suite, selftest, 63 native UX assertions and Focus
  regression; light/dark captures and rendered 300-point before/after comparison.
  Evidence and process-scale limitations are in local candidate validation.
- [ ] At least three artists complete Sketch → Ink → Paint → Save/export without
  guidance; include real tablet hardware and record wrong-panel searches,
  misclicks, obstruction and device/driver details. Resolve severe findings.
- [ ] Complete supported Windows display-scaling, accessibility and physical pen
  acceptance. Local process-factor screenshots alone do not close this gate.

The recording-soak, clean-machine and supported-hardware gates remain required.
Per-candidate build, dependency/source and archive proof is recorded in
local-candidate-validation.md.

## Recording timelapse continuation

Implementation covers the separate Recording dock, configurable PNG capture,
local multi-session/Save As timelines, portable record v3 and configurable
MP4/GIF export. Automated and native-window evidence is recorded in local
candidate validation. The 4000×4000 median-point benchmark currently exceeds
the agreed 10% regression threshold and remains an open acceptance gate.
The eight-hour actual drawing/recovery/export soak,
physical tablet and clean Windows gates remain open. Three-platform CI has
passed; the published identities passed their exact packaging checks.

## Native prerelease packaging — 2026-10-19

Windows installer/ZIP with the approved no-JBIG TIFF runtime, exact component
review and corresponding sources have been built and hash-verified. Native
macOS arm64 and Ubuntu 24.04 x86_64 builds/tests, startup probes and final
packaging passed in run 39199227884. The three-platform
[v0.3.0-beta.1 prerelease](https://github.com/Toji-ue/Prism/releases/tag/v0.3.0-beta.1)
is published with verified hashes and platform manifests. See the candidate ledger for
commit/hash identities and the open hardware, soak, installer and performance
gates. Signed/notarized production distribution remains separate.

The Ubuntu `.deb` installer was added at `0ce08f7` and passed packaging plus
APT install, installed-app startup and removal in run 39201746041. The release
includes it alongside Windows Setup and macOS DMG; the root README provides
download/install instructions. This CI result does not close clean-machine QA.

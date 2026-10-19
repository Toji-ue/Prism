# UX and closed-beta implementation contract

Approved 2026-10-13. Target: address the five release blockers and substantiate
all ten UX heuristics. An implemented feature is not a 40/40 score or a release
sign-off. Physical tablet, eight-hour drawing and clean-machine checks require
recorded evidence.

## Required behavior

1. Artwork: independent saved-project and autosave revisions; stable history
   checkpoints; a bounded 45-second autosave deadline unaffected by later edits;
   committed-only snapshots, newest and previous atomic recovery files; visible
   I/O failures. Save/Discard/Cancel guards Close/Quit/New/Open/import/recovery.
   Ctrl+S uses the project path, Ctrl+Shift+S is Save As; flat export is not Save.
   Save acknowledges the captured revision, never later edits. Discard cleans
   only this document's recovery after its worker finishes, retaining recordings.
2. Recording: always enabled, read-only status; failures visible, retry preserves
   valid prefix/hash/timestamps, gaps stay acknowledged. Normal export requires
   the final frame to be acknowledged; explicit partial export is labeled.
   Session metadata distinguishes active/completed/crashed/error/archive states.
   Storage lists sessions, sizes and status, offers export/open folder/confirmed
   deletion of inactive sessions. No automatic retention cleanup.
3. Document setup: pixel/inch/mm conversions and actual DPI (72 default, actual
   300 for that preset); pixel mode preserves pixels on DPI changes, physical
   modes preserve physical size. Clipboard retains pixels and alpha. Paint-layer
   count excludes an optional solid raster background. Remove unsupported
   description/fill/canvas-color options; RGB/Alpha, 8-bit, sRGB only. Size presets
   persist width/height/DPI. Validate and stage new documents before replacement.
   Flat-export DPI metadata matches the document without resampling pixels.
4. UX: actionable persistent errors and brief success on Start/full/focus screens;
   progress and duplicate-request protection. Notices avoid the pen; extended
   notices wait for stroke end. Discoverable Quick Controls/Brush Lab; compact
   Colour by default, optional persisted pinned picker. At least 32 logical px
   primary toolbar/layer targets, focus/hover/accessibility; grouped tools. Offline
   F1 Help covers drawing, saving, recovery, recording and current shortcuts.
   Keep shared themes/corner tokens, square artwork, usable minimum-size panels.
5. Candidate: consistent version and build/payload SHA-256 manifest, verify ZIP
   and installer payload against the tested Release; current tester instructions;
   runtime licenses/notices/provenance; Windows clean install/upgrade/uninstall
   protects user data. Windows CI covers compatibility and UI interactions;
   macOS/Linux build/core-test gates. Do not claim public/platform readiness.

## Verification

Behavioral tests at History, controller/file I/O, recorder/export and real QML
interaction boundaries. Test sustained edits, save checkpoints/branches/eviction,
failed/cancelled saves, outgoing worker results, atomic failure/crash recovery,
storage/encoding faults, final-frame and partial exports, units/DPI/clipboard and
old formats, notices/keyboard/input conflicts. Native light/dark screenshots at
1280x780 and 1000x620, Windows scale 100/125/150/200%. Measure 300-point rendering
benchmarks through 4000x4000 and responsiveness with autosave/export. Record a
physical-tablet completed illustration and eight-hour recording/recovery/export.
Run independent Standards and Spec review, then re-score all ten UX heuristics.

## Constraints

Qt/QML and current renderer/brush pipeline; existing file-I/O worker pattern,
no render/brush threading or GPU dependency change. Native project/brush/record
formats and old loaders/hash inputs remain compatible. Recordings are retained
until explicit deletion. Compact shelf with optional pinned picker is the plan's
default. Commit only task-related work; preserve unrelated workspace files.

## Implementation ledger

- [x] Artwork state, guarded transitions and robust autosave. Local automated coverage; external acceptance remains below.
- [x] Recorder failure/retry/coverage, final-frame export and Storage. Local automated coverage; external acceptance remains below.
- [x] Honest document setup and DPI metadata. Local automated coverage; external acceptance remains below.
- [x] Notifications, accessible controls, colour pinning and offline help. Local automated coverage; external acceptance remains below.
- [x] Candidate packaging code, notices, CI workflows and local validation documentation. Exact release identities and platform source/archive checks are recorded in `local-candidate-validation.md`.
- [x] Local automated/pixel/interaction verification and independent Standards/Spec review with focused rechecks. This does not establish a UX score or physical hardware acceptance.
- [ ] Physical pen, eight-hour real drawing, clean-machine/platform acceptance.

## Continuation after interruption

Async retry and archive recovery use the existing file-I/O worker, with a busy state and no GUI wait for spool scans. Publishing the new index happens through a queued callback. Transitions preserve active pointer grabs and match the final capture to its spool. Cancelling a failed recording close keeps artwork recovery. Completed sessions do not appear as crash recovery, and a clean crash prefix still has incomplete coverage without completion metadata.

Packaging code, runtime notices and CI jobs are implemented. Three-platform CI
has passed, and reviewed Windows candidates include corresponding source and
verified installer/ZIP hashes. Native packaging passed in run 39199227884;
the published v0.3.0-beta.1 identities are recorded in
`local-candidate-validation.md`. Physical hardware, eight-hour and clean-machine
acceptance remain open.

## Artist workflow UX — approved 2026-10-15

Keep full workspace on document entry, docked Layers over Colour, Q/Ctrl+1/2/3,
session-only Focus and always-on retained recording. No engine/backend or file
format changes are part of this work.

- Choose/Edit brush views: searchable named previews, All/Pinned/Recent, stable
  preset identity and Modified state, fixed real-engine preview, Basic first,
  collapsible Tip/Dynamics/Graphite, Save copy/Reset and labelled management.
  Delete requires confirmation. Import applies the selected settings. Thumbnail
  identity must survive reorder/delete; preset switching loads stored settings.
- Colour: independent hue ring, SV square, shared remembered neutral hue,
  validated HSV/Hex with Enter/Escape, and pen-sized palette. Tool Options opens
  Edit Basic. Flow is labelled Flow.
- Common themed buttons/fields/checks/tabs and 32 px slider hit regions. Numeric
  entry and keyboard nudging must remain usable; unfocused wheel scrolling must
  not alter a slider. Focused controls scroll into view, and Home/End/arrows reach
  every brush in long lists with separate Select/Pin keyboard targets. Q replaces
  any panel, keeps a 24 px pen gap and viewport bounds, and links to All
  brushes/Edit. Focus keeps IDs 0/1/2 and no duplicates.
- Start Page uses compact empty/recent states and keyboard-accessible New/Open,
  recovery and missing-file removal. Notice position stays fixed; source-tagged
  errors expose only appropriate Save As/autosave/recording/partial actions.
- Interaction gates include resize entry/undo, Tab navigation within controls,
  preset edits/import/management, numeric colour/Hex cancellation and exclusivity.
  Rendered evidence covers both themes, 1280x780/1000x620 and process factors
  1/1.25/1.5/2; this does not replace changing Windows display settings. Full
  Release/selftest and 300-point rendering benchmarks are required.

Local results belong in local-candidate-validation.md. Physical tablet and at
least three artists completing Sketch → Ink → Paint → Save/export remain pending;
record wrong-panel searches, misclicks and canvas obstruction before accepting UX.
Eight-hour recording, clean Windows and supported-hardware acceptance stay open.
CI and component/source/archive results are recorded per candidate in the ledger.

## Recording workflow — approved 2026-10-16

Always-on timelapse uses 1.5-second default capture and 1920 px long edge,
configurable to 0.5–60 seconds and 720/1920/Original. The separate 320 px right
Recording dock preserves Layers/Colour, defaults closed and hides in Focus.
Stored-frame preview loads on opening; explicit Refresh updates its snapshot and
a scrubber reads stored frames.

Versioned local timeline links use saved-file fingerprints. Save As freezes and
branches shared history; imported/moved artwork links explicitly. v3 portable
record bundles preserve v1/v2 source bytes and branch references. Raw deletion
must not bypass shared-source protection. No project/brush format changes or
new codec dependencies are authorized.

Docker/Storage share video settings: two-minute default, 30 FPS, 1920 px, five
seconds holding the end; presets 1/2/4 minutes, 12/24/30 FPS and 1280/1920 px.
Input FPS and optional intro are advanced controls. Total video stays at most
240 seconds; record bundles retain all data. Progress, cancellation, final-frame
acknowledgment and atomic output are required. Missing completion evidence or
gaps require explicit Partial export. See recording-format.md for compatibility
and input bounds; local-candidate-validation.md records measured acceptance.

Physical pen, eight-hour drawing, clean installation and supported-hardware
performance remain open. Exact platform runtime/source reviews and final
archive validation are required for each distributed identity; see the current
candidate ledger.

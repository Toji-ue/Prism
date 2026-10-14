# AGENTS.md

Guidance for AI coding agents (Claude Code, Copilot, Cursor, etc.) working in this repo.

**Status**: greenfield. The decisions below are the plan of record — the codebase is being built toward them. When reality diverges, update this file in the same change; it is a living doc.

**If this file and the code disagree**: treat the code as ground truth for *what currently exists*, and this file as ground truth for *what it should become*. Don't silently follow stale guidance and don't silently follow stale code — flag the mismatch in the PR description and update this file in the same change. If the divergence affects an "Ask first" item below, stop and ask rather than resolving it unilaterally.

## Project overview

Prism is a native desktop drawing/illustration app in the spirit of Clip Studio Paint / Photoshop: layered canvas painting, a custom brush engine, vector + raster tools, and local project files with an optional cloud layer (accounts, licensing, asset store, cloud backup).

- **Primary users**: illustrators, comic/manga artists, concept artists, working on pen tablets and pen displays (Wacom, Huion, XP-Pen).
- **Platforms**: Windows first; macOS/Linux stay buildable. Tablet OSes (iPadOS/Android) are out of scope for now.
- **Non-goals**: video editing, 3D sculpting, print-shop color workflows.

## Tech stack (plan of record)

| Area | Decision |
| --- | --- |
| Language | C++20 |
| UI | Qt 6 + QML (Qt Quick) |
| Rendering | Qt RHI (Vulkan / D3D / Metal underneath) |
| Build | CMake + CMake Presets |
| Dependencies | vcpkg |
| Image processing | Skia, or custom GPU routines where appropriate |
| Color management | LittleCMS |
| PSD | libpsd or a maintained PSD library |
| Raster codecs | libpng / libjpeg-turbo / libwebp |
| Vector | custom representation + GPU rendering |
| Tablet input | Qt tablet events; platform-native APIs (Wintab/WinInk) where Qt falls short |
| Undo/redo | custom command/history system |
| Project format | custom, versioned |
| Testing | GoogleTest + Qt Test |
| Formatting / static analysis | clang-format / clang-tidy |
| CI | GitHub Actions |
| Cloud backend (later, separate service) | TypeScript/NestJS or Go · PostgreSQL · S3-compatible storage · Auth0/Clerk/custom OAuth |

This repo is client-only; backend code will live elsewhere. New dependencies need a note here explaining why they earned their place.

### Cost / free-to-develop notes

Everything in the table above is $0 to develop with — no seat licenses, no paid SDKs required to build and test the app. Two items have strings attached, not costs, and are worth stating explicitly so nobody trips over them later:

- **Qt 6 is free under LGPLv3** ("Qt for Open Source Development"), not free-and-unencumbered. To ship a closed-source commercial build under LGPL (no Qt commercial license), Qt must be **dynamically linked**, and users need the ability to relink against a modified Qt (e.g. by shipping Qt as separate DLLs/.so files, not baked into a single static binary). Static-linking Qt into the executable requires a paid Qt commercial license. This constrains packaging/distribution, not development — call it out explicitly in the build/packaging setup so it isn't discovered at release time.
- **GitHub Actions is free with limits, not unlimited.** Public repos get unlimited free minutes; a private repo gets a fixed free minutes/month allowance on the free plan, then bills per minute. If the repo is private, either budget for occasional CI cost at scale, or use a self-hosted runner (free, your own hardware) once usage grows.

Everything else — CMake, vcpkg, Skia, LittleCMS, libpsd, libpng/libjpeg-turbo/libwebp, GoogleTest, Qt Test, clang-format/clang-tidy — ships under MIT/BSD/Apache-2.0-style or LGPL terms with no dynamic-linking caveat beyond the one above, and no usage fees. A compiler/toolchain is also free: MSVC via Visual Studio Community (free for individuals, open-source projects, and small teams/organizations under Microsoft's revenue/headcount thresholds) or LLVM/Clang, both $0.

### License policy for dependencies

This is a commercial product — license compatibility is not optional.

- **Fine by default**: MIT, BSD (2/3-clause), Apache-2.0, zlib/libpng, ISC.
- **Needs review before adding**: LGPL (dynamic linking only, and confirm the build actually links dynamically), MPL-2.0, any "source-available" or dual-licensed package.
- **Do not add without explicit sign-off**: GPL/AGPL in any form that would touch code we ship (static linking, header-only inclusion, copy-pasted snippets), and anything with a non-commercial or field-of-use restriction.
- Record the license of every new `third_party/` or vcpkg dependency next to its entry in the dependency table or in `third_party/README.md`. If a transitive dependency pulls in something in the "review" or "sign-off" tier, treat it the same as adding it directly.

### vcpkg workflow

- Dependencies are declared in `vcpkg.json` (manifest mode) — don't install packages ad hoc into a shared triplet.
- Commit `vcpkg.json` and the lockfile (`vcpkg-configuration.json` / baseline) together so builds are reproducible; don't hand-edit the baseline hash.
- Pin versions explicitly for anything touching rendering, color management, or file format parsing (Skia, LittleCMS, PSD, codec libs) — floating on "latest" for these risks silent behavior changes in pixel output.
- When adding a package, note in the same PR: what it's for, its license (see above), and rough binary-size/startup impact if non-trivial.

## Repository structure (target layout)

```
src/
├── core/
│   ├── document/
│   ├── layer/
│   ├── history/
│   ├── brush/
│   └── geometry/
│
├── canvas/
│   ├── renderer/
│   ├── compositor/
│   ├── brush/
│   ├── shaders/
│   └── gpu/
│
├── ui/
│   ├── qml/
│   ├── panels/
│   ├── dialogs/
│   └── application/
│
├── io/
│   ├── project/
│   ├── png/
│   ├── jpeg/
│   ├── psd/
│   └── export/
│
├── platform/
│   ├── windows/
│   ├── macos/
│   └── linux/
│
└── app/
    └── main.cpp
/tests
/third_party      # vendored or pinned external deps
/cmake            # CMake modules/toolchains
```

Keep `/core` free of Qt/GPU types so domain logic stays unit-testable. Place new code in the module whose name it matches; add the module here when you create one.

## Commands

```bash
cmake --preset default          # configure
cmake --build --preset default  # build
ctest --preset default          # run tests

powershell -File tools/deploy.ps1   # deploy to deploy/ (stops the app, verifies the hash)
powershell -File tools/package.ps1  # versioned, hash-bound candidate; distribution license gate

clang-format -i <files>         # format
clang-tidy -p build <files>     # lint
```

Add presets in `CMakePresets.json` rather than inventing ad-hoc flag combinations.

**Qt toolchain (MSYS2 UCRT64).** PNG lives in QtGui; every other image format is a plugin, and
`mingw-w64-ucrt-x86_64-qt6-base` ships only JPEG/GIF/ICO. Run
`pacman -S mingw-w64-ucrt-x86_64-qt6-imageformats` once or the build cannot read WebP/TIFF/TGA at
all — `tools/deploy.ps1` copies whatever plugins exist and then fails on `--check-formats` if the
essentials are missing, so a machine without the package cannot ship a silently crippled installer.

## Core domain concepts

Understand these before touching canvas, rendering, or history code:

- **Document**: canvas size, DPI, color profile, layer tree.
- **Layer**: raster, vector, or group; blend mode, opacity, clipping, lock state, transform. Layers form a nestable tree.
- **Brush engine**: converts tablet input (position, pressure, tilt, rotation, velocity) into strokes, per preset (size, opacity jitter, texture, dynamics curves). The most performance- and correctness-sensitive code in the repo.
- **Current brush pipeline**: `BrushStroke` places stamps at continuous path-distance intervals and shares that path for live preview, committed strokes, thumbnails, and offline replay. Solid keeps the prior stamp behavior. Graphite uses canvas-anchored procedural grain, minimum 75% light-pressure width, distance-normalized deposit, time-smoothed pressure, and tilt-driven broad shading. The built-in HB and 2B presets use Graphite; Ink and Paint remain Solid. Paint does not simulate wet pigment mixing yet.
- **Brush preset format**: `.prismbrush` is version 5. It adds `endTaperDiameters`; built-in Ink tapers at release, while HB/2B and imported v1–v4 brushes default to a round end. Version 4 added Graphite material, grain, grain scale and tilt shading; v1–v3 load as Solid. User presets persist all of these fields.
- **Workspace modes**: every entry into the drawing workspace starts with the full interface. It keeps Layers above a compact Colour shelf, with Brush Lab on `Ctrl+2`; Start Page and workspace share the same light/dark surfaces, typography and 12/8/6 px panel/control/chip corner tokens while artwork pixels remain square. The toolbar icon with four rounded corners and `Tab` toggle the same session-only focus mode, hiding Prism chrome except for a 32 px workspace-layout restore icon at the top right. `Ctrl+1/2/3` opens Layers, Brush Lab, or Colour as a popover while focused; `Q` opens configurable Quick Controls near the pen in either mode. A stroke closes popovers and hides the restore icon, while text inputs keep their keys for editing. Quick Controls stores recent colours and stable-ID pinned/recent presets in user settings without changing `.prism` or `.prismbrush`. Colour can be pinned in user settings; F1 opens offline help and shared actionable notices work on Start Page, full workspace and focus mode. Primary toolbar/layer targets are at least 32 logical pixels. Built-in preset slots stay fixed; users can reorder or delete user presets, but cannot move them across the built-in boundary.
- **Artwork sessions**: `ArtworkSession` owns immutable asynchronous save/recovery jobs; History state IDs distinguish native Save from autosave and flat export. Autosave has a 45-second deadline that later edits do not restart and retains newest/previous atomic snapshots. All live brush, move and opacity previews block snapshots until commit. Production document transitions use Save/Discard/Cancel, preserve the current pointer grab until release, and defer recovery deletion until a close is actually accepted. DPI/units/clipboard in New Document are real; background is transparent or a separate solid raster layer.
- **Project format**: `.prism` v2 stores stable layer IDs, nested group parent IDs, clipping, alpha lock and lock flags. The v1 loader remains supported. Group, clipping and alpha-lock edits, plus Resize Canvas, are history commands. Autosave writes a separate `.prism` snapshot in the recovery directory; the Start Page offers it without overwriting the original.
- **Drawing record format**: recording starts automatically for every document and has no user pause/stop control; its controller status is read-only. Recovered recordings remain immutable for export; returning to the drawing workspace starts a live recording session. `.prismrec` v2 is newline-delimited JSON: a header, then independently flushed PNG frames with the existing hash chain. A file-I/O worker encodes and appends frames; the GUI keeps offsets and hashes rather than frame images. Crash recovery accepts the complete valid prefix of a truncated spool. Strict verification still reads v1 and v2. GIF/MP4 select at most 2,880 frames (about four minutes at 12 fps); `.prismrec` keeps every recorded frame. UI exports use a separate worker and an immutable spool index so document replacement cannot change the frames being exported.
- **Recording lifecycle**: I/O failure is visible; asynchronous retry/recovery reuse the file-I/O worker and publish indexes on the GUI thread. Capture IDs are meaningful only with their spool identity. Normal exports wait for final-frame acknowledgment; crashes or missing completion metadata require explicitly partial export. Recovered archives and their metadata remain immutable. Completed recordings belong in Storage, not crash recovery. Storage retains recordings until confirmed deletion and forbids deleting sessions in use.
- **Compositing**: layers blend bottom-to-top into the final image. Recompute only dirty regions per stroke tick, using cached composites.
- **History**: every mutating action is a reversible command/diff in the history system — pixel data is never mutated in place with no record.
- **File format**: the native project format round-trips losslessly (layers, blend modes, vector data). Flatten only on explicit flat export (PNG/JPEG). Version the format; every version's loader keeps working.
- **Color**: sRGB today, ICC-aware via LittleCMS. Color-space conversions are explicit; never silently clamp or convert.
- **Tablet input**: pressure curves, tilt, palm rejection are core UX. Any input-handling change must state whether it was tested on real tablet hardware.
- **Collaboration (planned, not implemented)**: the current source has no `CollabSession` or LAN room. If this is built later, synchronize finished strokes with the shared `BrushStroke` replay path and add a two-peer interaction test before documenting any collaboration behavior as supported.

## Known gotchas

Mistakes that are easy to make in this codebase and easy to miss in review:

- **Full-canvas recompute on a single-layer change.** A visibility/opacity toggle, a blend-mode change, or a single stroke tick must only touch the dirty region's composite cache, not re-rasterize the whole 4000×4000 canvas.
- **Mutating pixel data without a history entry.** Any code path that writes into layer pixel data outside the command/history system will silently break undo — this is a correctness bug, not just a UX gap.
- **Confusing opacity, flow, and alpha.** They govern different things in the brush/compositing pipeline; using the wrong one compiles fine and produces a subtly wrong stroke or blend.
- **Allocating on the stroke-point or per-frame hot path.** Even a single `std::vector` growth or `shared_ptr` churn per input event can push latency over the 16 ms budget on lower-end tablets.
- **Silent color-space conversion or clamping.** Any conversion between color spaces (or clamping out-of-gamut values) must be explicit and visible in the diff — never a side effect of an unrelated change.
- **Breaking an old project-format loader while adding a new one.** Every format version's loader must keep working; "just bump the version and stop supporting the old one" is a data-loss bug for existing users' files.
- **Testing tablet/GPU-path changes only on a mouse or in a VM.** Pressure curves, tilt, and palm rejection behave differently on real hardware; a change that "feels fine" with a mouse can still be untested for its actual purpose.
- **Treating a QML argument as a file path.** QML hands file arguments over either as a `url` (`FileDialog.selectedFile`) or as a plain string (scripts, drag-and-drop, `QMetaObject::invokeMethod`). `QUrl("C:\\x.png")` parses `c` as a scheme and percent-encodes the separators, so `toLocalFile()` returns empty or `C:%5Cx.png` and the operation silently dies with "no file name specified". Route every file-taking invokable through `localPathFrom()` in `AppController.cpp`; the selftest covers this.
- **Assuming a deploy happened.** A running `prism.exe` locks `deploy\prism.exe`, so `Copy-Item` fails with an IOException that is easy to lose in a long build log — leaving a stale binary that "does not have" the change just made, which reads as a broken feature rather than a failed copy. Always deploy with `tools/deploy.ps1` (stops the app, retries, verifies the hash) and check the hash before reporting a feature as shipped.
- **Canvas coordinates vs layer-buffer coordinates.** A crop moves the canvas *window* inside the layer buffers (`Document::canvasOrigin()`) so the cropped-away pixels survive in all four directions: `layer.pixels()` is then larger than the canvas and its (0,0) is not the canvas' (0,0). The app speaks canvas coordinates everywhere; anything reading or writing layer pixels directly goes through `AppController::toBuffer()` / `toCanvas()` and passes the offset to `clipToSelection` (its mask is canvas-sized). `Compositor::composite()` takes canvas coordinates and applies the origin itself. A layer created after a crop must be allocated canvas-at-origin, as `Document::addRasterLayer` does.
- **Changing what a drawing record hashes.** `SessionRecorder` hashes `previousHash + ms + frame PNG` for every frame of a session, and `--check-record` recomputes that chain so a client can verify a `.prismrec` without Prism. Any change to the hash input, the frame encoding or the JSON field names silently invalidates every record already handed to someone — bump `kRecordVersion` and keep the old verifier working instead. And be honest about what it proves: that the file is the sequence this app wrote, not who drew it.
- **Media Foundation reads RGB32 buffers bottom-up.** That is the DIB convention: with a positive stride the first row in memory is the *bottom* row, so an RGB32 frame copied straight out of a `QImage` encodes upside down. The first MP4 export shipped that way and nothing caught it, because `--selftest` only counts decoded samples. `RecordExport::writeMp4` now flips every frame (`flipped(Qt::Vertical)`) before locking the buffer. Note why the existing check missed it: `verifyVideo` decodes through our own MF path, so encoder and decoder agree on a wrong orientation and the round-trip still passes. Orientation needs a decoder we did not write — decode with ffmpeg and compare the frames against the `.prismrec` of the same session, flattening the record's transparent paper to white first (`tools/check-record-export.py` does exactly this).
- **A `Canvas` icon inside a `Repeater` inside the window's `ToolBar` never gets its texture.** The buttons report the right size, sit at the right coordinates, hover, hit-test and log an `onPaint` with the right name and size — and draw absolutely nothing. It cost a whole evening of "the record button is missing" (it was never missing, it was invisible) and the fix is boring: write the toolbar buttons out (`ToolbarButton.qml`) instead of generating them. Two checks came out of it, keep both: `--demo-record` asserts the automatic recording indicator has a size and lands inside the window ("on screen: true"), and the toolbar icons are visible in any `--screenshot`, so a regression is one crop away. Repeaters elsewhere (toolbox, layer docker) are fine — it is this ToolBar/contentItem creation order, and a plain `Rectangle` in the same delegate still draws, which is what makes it look like a phantom.
- **A drawing record belongs to ONE document.** `SessionRecorder` keeps its frames until something clears them, so any path that replaces the document (`createDocument`, `openProject`, `importImage`) must call `AppController::forgetRecordedFrames()` — otherwise an export in the second document glues the first drawing in front of it, frames of a different canvas size included, and the file looks like two pictures spliced together. The selftest checks it by exporting right after `newDocument()` and requiring the first frame to be blank at the new document's size.
- **An export has to end with what is on the canvas.** `exportRecording()` forces a final frame capture and flushes the disk worker before reading the spool. The old 400-frame memory limit is gone; never reintroduce a silent cap on `.prismrec`. GIF/MP4 may sample the whole timeline down to the four-minute timelapse limit, retaining the first and last frames.

## Performance constraints

Drawing apps live or die on input latency. In rendering, input, or compositing code:

- Target sub-16 ms (60 fps) pen-to-pixel latency; sub-8 ms is the "no lag" bar on supported hardware.
- Keep heap allocation out of hot paths (event handlers, per-stroke-point processing, per-frame compositing).
- Dirty-rect / partial redraw only; a layer toggle on a 4000×4000 canvas must never trigger full re-rasterization.
- Run large filters, file I/O, and export off the UI/render thread.
- PRs touching the render loop, compositing, or input pipeline say so explicitly, with before/after latency or frame-time numbers where obtainable.

### Measured baseline (Windows, 1280×800 … 4000×4000 canvases)

`prism.exe --demo-stroke-bench=300 --demo-doc-size=WxH --screenshot-only --screenshot=<file>` drives 300
real mouse moves through `QTest` and reports frame time, texture-upload volume and a CPU split
(`beginStroke` / moves / `endStroke`). Numbers measured on the development machine:

| Canvas | per move (before → after) | `endStroke` (before → after) | erase `beginStroke` (before → after) | texture uploads |
| --- | --- | --- | --- | --- |
| 1280×800 | 1.25 → 0.59 ms | 141 → 11 ms | 16 → 1 ms | 30 × 4.1 MB → 106 × 256 KiB |
| 2000×3000 | 10.07 → 1.02 ms | 495 → 59 ms | 93 → ~7 ms | 50 × 24 MB → 321 × 256 KiB |
| 4000×4000 | unusable → 1.07 ms | 1207 → 160 ms | 264 → 15 ms | 42 × 64 MB → 483 × 256 KiB |

Three rules fall out of that table, all learned the hard way:

- **Upload only dirty tiles.** The canvas is a grid of 256 px `QSGSimpleTextureNode`s and
  `CanvasView::onCanvasChanged` marks just the tiles under the dirty rect. Uploading the whole canvas
  per frame meant a texture create/destroy pair plus a full RGBA upload every frame — 64 MB per frame on
  a 4000×4000 document, which is what made drawing feel laggy on weaker GPUs.
- **Never redo whole-canvas work on mouse-up.** `endStroke` commits the *accumulated* preview over the
  stroke's own rectangle (`m_strokeDirty`) and recomposites only that rect; shapes, gradients, fills and
  filters commit through `commitPatch`, which does the same. Re-painting the stroke into a fresh
  canvas-sized buffer and recompositing every layer cost 1.2 s of freeze at 4000×4000. Reusing the
  preview is valid precisely because `extendStroke` stamps the same `paintSegment` calls
  `BrushStroke::paint` would; if that invariant changes, the dot stamped in `beginStroke` and the commit
  both need revisiting (a click with no movement is covered by the selftest).
- **Edit the layer, keep one snapshot.** The eraser used to hold two full copies of the layer (a write
  buffer plus a compositor layer override) before painting anything. It now erases into the active layer
  live, keeps only `m_eraseBefore` for undo, and writes the selection-clipped patch back at commit.

## Code style

- Follow the repo's `.clang-format`; format before committing.
- RAII for GPU resources, file handles, locks — early returns and exceptions must not leak.
- Put canvas/rendering math (blending, curve interpolation, transforms) in small, pure, testable functions, separate from GPU/draw-call code.
- Use domain terms precisely — *opacity* vs *flow* vs *alpha* are different things, and confusing them produces hard-to-spot bugs.
- Comments explain *why*, especially performance workarounds (e.g. "batched: per-point submission caused visible lag on integrated GPUs").
- Naming: `PascalCase` for types (classes, structs, enums), `camelCase` for functions and local variables, `m_camelCase` for private member variables, `SCREAMING_SNAKE_CASE` for compile-time constants and macros. Namespaces are lowercase and match the directory under `src/` (e.g. `core::layer`, `canvas::compositor`). One primary type per file; file name matches the type (`Layer.h` / `Layer.cpp`).

## Testing expectations

- Unit tests for: color math, blend mode formulas, file format (de)serialization, history/undo-redo logic, coordinate/transform math.
- Pixel-diff / golden-image tests for compositing output — canvas bugs slip past code review.
- Changes touching tablet input, brush rendering, or GPU paths state in the PR which hardware/platforms they were tested on.
- File-format changes ship with round-trip tests; a format bug corrupts users' artwork.
- **Testing GPU/rendering code**: keep the pure math (blending, curve interpolation, transforms, color conversion) in `/core`, free of Qt/GPU types, and unit-test it directly with plain inputs/outputs — no GPU context needed. For code that does touch the GPU (compositor, shaders, Qt RHI calls), isolate the draw-call/resource-binding layer behind a thin interface so it can be swapped for a software/mock backend in tests; reserve actual on-GPU golden-image tests for the compositing pipeline itself, run where a GPU is available in CI, and skip (not silently pass) where it isn't.
- **Interaction bugs need interaction tests.** Property-level tests pass happily while a control is undriveable (a slider whose value fights a binding, a row that cannot be dragged). The `--demo-drag-slider=<objectName>`, `--demo-drag-layer=FROM:TO` and `--demo-click=<objectName>` hooks drive real mouse input through `QTest::mousePress/mouseMove/mouseRelease/mouseClick`; use them rather than `QCoreApplication::sendEvent`, which bypasses the delivery agent's grab bookkeeping and reports a working drag as broken. Give any control you want to verify this way an `objectName`, and look items up through the **visual** item tree (`findItemByName` in `main.cpp`) — `QObject::findChild` cannot see items created by a `Repeater` or a view delegate.
- **Layer index vs. screen order.** Layer index 0 is the BOTTOM of the stack (the compositor paints 0..n-1 upwards), while Krita-style dockers show the topmost layer first. The Layers list therefore runs `ListView.BottomToTop`, and any y-to-row maths in that view must go through the view's own helpers instead of dividing by the row height.

## Ask first

Get explicit sign-off before:

- Changing the native project format without a versioned migration path for existing files.
- Adding or swapping a rendering backend or GPU library.
- Introducing new threading around the render loop or brush engine — canvas apps are easy to make subtly racy.
- Changing default color space, DPI, or export behavior.
- Adding a heavy third-party dependency (check license compatibility — this matters for a commercial product — plus binary size and startup impact).

## Commit / PR conventions

- Conventional commits: `feat:`, `fix:`, `perf:`, `refactor:`, `test:`, `docs:`.
- Perf-sensitive changes (rendering, compositing, tablet input) are called out in the PR description with before/after numbers where possible.
- Platform-specific changes note which platform(s) they were built and tested on.

## Local candidate validation

For UX-beta completion evidence, review `docs/local-candidate-validation.md` and `docs/ux-beta-plan.md`. `PrismVersion.txt` supplies the compiled/runtime candidate version (do not use a root VERSION file: Windows resolves it instead of C++ `<version>`). Development deployment uses fresh dynamic Qt files, Release tests, executable/payload hashes and a source fingerprint. External packaging is separately blocked by the TIFF → libtiff → GPL jbigkit dependency and other unreviewed runtime licenses; see `third_party/README.md`. Do not silently remove codecs, claim distribution approval, or treat local CI files as completed remote CI runs.

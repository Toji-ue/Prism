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

clang-format -i <files>         # format
clang-tidy -p build <files>     # lint
```

Add presets in `CMakePresets.json` rather than inventing ad-hoc flag combinations.

## Core domain concepts

Understand these before touching canvas, rendering, or history code:

- **Document**: canvas size, DPI, color profile, layer tree.
- **Layer**: raster, vector, or group; blend mode, opacity, clipping, lock state, transform. Layers form a nestable tree.
- **Brush engine**: converts tablet input (position, pressure, tilt, rotation, velocity) into strokes, per preset (size, opacity jitter, texture, dynamics curves). The most performance- and correctness-sensitive code in the repo.
- **Compositing**: layers blend bottom-to-top into the final image. Recompute only dirty regions per stroke tick, using cached composites.
- **History**: every mutating action is a reversible command/diff in the history system — pixel data is never mutated in place with no record.
- **File format**: the native project format round-trips losslessly (layers, blend modes, vector data). Flatten only on explicit flat export (PNG/JPEG). Version the format; every version's loader keeps working.
- **Color**: sRGB today, ICC-aware via LittleCMS. Color-space conversions are explicit; never silently clamp or convert.
- **Tablet input**: pressure curves, tilt, palm rejection are core UX. Any input-handling change must state whether it was tested on real tablet hardware.

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

## Performance constraints

Drawing apps live or die on input latency. In rendering, input, or compositing code:

- Target sub-16 ms (60 fps) pen-to-pixel latency; sub-8 ms is the "no lag" bar on supported hardware.
- Keep heap allocation out of hot paths (event handlers, per-stroke-point processing, per-frame compositing).
- Dirty-rect / partial redraw only; a layer toggle on a 4000×4000 canvas must never trigger full re-rasterization.
- Run large filters, file I/O, and export off the UI/render thread.
- PRs touching the render loop, compositing, or input pipeline say so explicitly, with before/after latency or frame-time numbers where obtainable.

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
- **Interaction bugs need interaction tests.** Property-level tests pass happily while a control is undriveable (a slider whose value fights a binding, a row that cannot be dragged). The `--demo-drag-slider=<objectName>` and `--demo-drag-layer=FROM:TO` hooks drive real mouse input through `QTest::mousePress/mouseMove/mouseRelease`; use them rather than `QCoreApplication::sendEvent`, which bypasses the delivery agent's grab bookkeeping and reports a working drag as broken. Give any control you want to verify this way an `objectName`.

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

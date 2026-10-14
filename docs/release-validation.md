# Release validation — 2026-10-12

This records what was checked for the UI, artwork, brush and recording changes. It is not a production sign-off.

## Automated checks

- Windows/MSYS2 UCRT64: `cmake --build --preset default -j 4` and `cmake --build --preset release -j 4` succeeded.
- `ctest --preset default --output-on-failure`: 108/108 passed. The suite covers v1/v2 projects, nested group compositing, clipping, alpha lock, Resize Canvas history, v1–v5 brush import, Ink taper, Graphite behavior and preview blending.
- Release `prism.exe --selftest`: zero failures. This includes artwork autosave/recovery, readable Recent Files names, cached previews and missing-file state, Ink preview/commit/replay and undo/redo, `.prismrec` v1/v2 verification, truncated record recovery, more than 400 frames, GIF/MP4 readback and worker exports that survive document replacement. The test Temp directory was set to `build/` because the sandbox Temp location is not writable.
- Release `prism.exe --tablettest`: synthetic Qt tablet path passed (light to heavy alpha 80→255, width 29→45 px; graphite tilt width 21→53 px). This is not a physical tablet result.
- Release `prism.exe --tooltest`: all audited tools produced their expected effect (zero failures).
- `--demo-focus-mode-test`: Tab stayed in a text field, canvas Tab entered focus mode, Ctrl+2 opened Brush Lab, and an engine stroke closed the popover and hid the glass panel. `--demo-focus-hover=left|right|top|bottom` opened each panel and saved a screenshot. The tablet path calls the same engine stroke methods; a physical pen check remains below.
- Start Page and workspace screenshots were inspected at 1000×620 in light and dark. New/Open/Recent are legible; the full workspace and focus controls fit. This does not replace mouse and physical pen QA of every control.
- `tools/deploy.ps1` copied the final Release executable, verified its hash against the build and passed its deployed image-format probe.
- `tools/package.ps1` created a 51.2 MiB NSIS installer and a 52.9 MiB portable ZIP with the updated tester guide. The deployed executable passed `--selftest` with `PATH` restricted to Windows system directories. Installation and uninstall on a separate clean machine remain untested.

## Rendering-window stroke benchmark

Windows Release build, visible Qt window, 300 QTest mouse moves. CPU average and median are the CPU-only point phase; mouse-up is `endStroke`. Uploads are counted during the visible rendering phase, so their number varies with frame scheduling. All rows reported nonzero rendered frames and tile uploads, with 256 px tiles rather than full-canvas uploads.

| Brush | Canvas | CPU average / point | CPU median / point | Mouse-up | Tile uploads / bytes |
| --- | --- | ---: | ---: | ---: | ---: |
| HB Pencil | 1280×800 | 0.103 ms | 0.045 ms | 1 ms | 57 / 12.06 MiB |
| HB Pencil | 2000×3000 | 0.263 ms | 0.074 ms | 8 ms | 171 / 40.54 MiB |
| HB Pencil | 4000×4000 | 0.553 ms | 0.123 ms | 21 ms | 340 / 80.94 MiB |
| Clean Ink | 1280×800 | 0.097 ms | 0.043 ms | 3 ms | 58 / 12.31 MiB |
| Clean Ink | 2000×3000 | 0.123 ms | 0.032 ms | 12 ms | 215 / 51.54 MiB |
| Clean Ink | 4000×4000 | 0.220 ms | 0.055 ms | 24 ms | 330 / 78.44 MiB |
| Soft Paint | 1280×800 | 0.347 ms | 0.182 ms | 3 ms | 94 / 21.31 MiB |
| Soft Paint | 2000×3000 | 0.627 ms | 0.264 ms | 9 ms | 273 / 66.04 MiB |
| Soft Paint | 4000×4000 | 1.213 ms | 0.448 ms | 24 ms | 461 / 111.19 MiB |

Before the final compositor optimization, three 1280×800 runs of the deployed Graphite Sketch averaged 0.100, 0.103 and 0.100 ms/point (median of run averages 0.100). Three runs of the changed HB Pencil averaged 0.100, 0.103 and 0.100 ms/point. The final HB run above averaged 0.103 ms/point. The old binary did not report individual point medians, so a direct before/after comparison for that statistic is unavailable. Its 4000×4000 run averaged 0.580 ms/point and 20 ms on mouse-up; the final HB run averaged 0.553 ms/point and 21 ms. No material CPU regression was observed in these comparisons. The medians in the table are useful for tracking future builds, but occasional expensive points make averages higher.

## Open release evidence

- Draw HB, 2B, Ink and Paint on a physical tablet and record model, driver, pressure/tilt capability, touch/lift, layering, fast and slow strokes, shading and perceived latency.
- Run an actual eight-hour recording and interrupted export/recovery soak. The automated test passes more than the former 400-frame cap, but does not simulate eight hours of real drawing.
- Measure UI responsiveness during an actual eight-hour export. The UI now sends streamed copy/encoding and readback verification to a worker, while it waits briefly for the final captured frame to reach the spool before starting that worker.
- Exercise mouse and pen interactions, recent-file missing state, dialogs and crash recovery on a clean Windows install. Repeat UI review at normal and minimum sizes on supported display scaling.
- Run clean-machine packaging, CI, macOS/Linux builds, accessibility and license checks before closing the beta or production gates.

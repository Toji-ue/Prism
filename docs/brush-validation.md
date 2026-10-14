# Brush and workspace validation (2026-10-12)

This page records the earlier Graphite v4 baseline on 2026-10-12. At that point `.prismbrush` v4 added material, grain, grain scale and tilt shading, while `.prism` remained unchanged. The current implementation uses `.prismbrush` v5 and `.prism` v2; see [release validation](release-validation.md) for the new checks and performance comparison.

## Automated checks

- `ctest --preset default --output-on-failure`: 98/98 passed. These include spacing across different sample rates, low pressure, live versus offline strokes, stabilization responsiveness, undo/redo, and old/new brush preset round trips. Graphite checks cover rounded light ends, constant-pressure ends, a low-pressure click, layer buildup, tilt width, preview, time-based pressure smoothing, and v1–v4 imports.
- `prism.exe --selftest`: passed with zero failures, including click strokes, preview/commit behavior, legacy presets, image tips, and pressure dynamics.
- `prism.exe --tablettest`: synthetic Qt tablet event path passed; alpha rose from 80 to 255 as pressure rose from 0.15 to 1.0, width rose from 29 to 45 pixels, and an HB Pencil tilt event widened a dot from 21 to 53 pixels. This is a software test, not a physical pen test.
- `prism.exe --demo-focus-mode-test`: text input retained Tab, canvas Tab entered focus mode, and Ctrl+2 opened the Brush Lab popover.
- Light and dark screenshots inspected at the window's normal and minimum sizes, including Brush Lab, tool flyout, menu, and New Document dialog.

## Release benchmark

Windows development machine, 300 QTest mouse moves. CPU figures exclude the asynchronous GPU render loop. The old binary came from the pre-change deployment; the new binary uses this change.

| Canvas / brush | CPU per move | Mouse-up |
| --- | ---: | ---: |
| 1280×800, old Prism Pen | 0.103 ms | 1 ms |
| 1280×800, new Prism Pen | 0.110 ms | 2 ms |
| 2000×3000, new Prism Pen | 0.130 ms | 8 ms |
| 4000×4000, old Prism Pen (repeat) | 0.333 ms | 24 ms |
| 4000×4000, new Prism Pen | 0.400 ms | 28 ms |
| 4000×4000, Graphite Sketch | 0.587 ms | 22 ms |
| 4000×4000, Clean Ink | 0.253 ms | 23 ms |
| 4000×4000, Soft Paint | 1.470 ms | 28 ms |

The 4000×4000 Prism Pen CPU path is about 0.067 ms per move slower than the repeated old run, while remaining under the historical 1.07 ms per move development baseline in `AGENTS.md`. The benchmark's hidden window reported zero render frames and uploads, so it cannot validate GPU upload volume. The dirty-tile upload code was not changed; texture upload should be measured in an actively rendered window before calling that part of the performance gate complete.

## Physical tablet acceptance

Pending: draw light and layered Sketch strokes, slow and fast Ink strokes with tapered ends, and large overlapping Paint strokes on a real Wacom, Huion, or XP-Pen device. Record model, driver, Windows pen mode, display refresh, pressure feel, perceived latency, and any jitter. The synthetic Qt tablet test above does not establish physical tablet feel.

## HB/2B Graphite benchmark

Windows development machine, Release build, normal visible rendering window, 300 QTest mouse moves. The old binary is the preceding Graphite Sketch in `deploy/prism.exe`; the new binary is HB Pencil. CPU/move is the median of three runs of the 300-move CPU phase at 1280×800, and one run at each larger size. The visible-phase total depends on how many asynchronous frames Qt renders, so it is reported separately.

| Canvas | CPU/move old → new | Mouse-up old → new | Visible frames old → new | Texture upload old → new |
| --- | ---: | ---: | ---: | ---: |
| 1280×800 | 0.163 → 0.153 ms | 2 → 2 ms | 17 → 20 | 18.31 → 20.06 MiB |
| 2000×3000 | 0.407 → 0.387 ms | 16 → 12 ms | 19 → 25 | 55.04 → 57.04 MiB |
| 4000×4000 | 0.913 → 0.860 ms | 31 → 32 ms | 40 → 26 | 102.44 → 96.19 MiB |

No full-canvas upload appeared; the 256 px tile upload path remained in use. The 1280×800 visible phase varied between 10 and 30 frames across runs, so total wall time per move is not a stable latency measure here. The benchmark currently reports average CPU time per point within each run, rather than the exact per-point median requested by the acceptance plan. Real tablet testing remains pending: model, driver, tilt support, light touch, stacking, fast/slow strokes, and side shading have not been measured on hardware.

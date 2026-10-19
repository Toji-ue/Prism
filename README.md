# Prism

A native drawing and illustration app for **Sketch → Ink → Paint**, built with C++20 and Qt Quick.

Open source under the [MIT License](LICENSE).

Prism combines a canvas-focused workspace, graphite and ink brushes, layers, local artwork recovery, and automatic timelapse recording.

## Download

**Current version: [0.3.0-beta.1 — prerelease](https://github.com/Toji-ue/Prism/releases/tag/v0.3.0-beta.1).**

| Platform | Installer | Requirements |
| --- | --- | --- |
| Windows | [Windows Setup (.exe)](https://github.com/Toji-ue/Prism/releases/download/v0.3.0-beta.1/Prism-0.3.0-beta.1-8deb20e0cc47-2afe57ce7296-Setup.exe) | Windows 10/11, x64 |
| macOS | [macOS disk image (.dmg)](https://github.com/Toji-ue/Prism/releases/download/v0.3.0-beta.1/Prism-0.3.0-beta.1-macos-arm64-93e470551b08.dmg) | macOS 14+, Apple Silicon; no Intel build |
| Ubuntu | [Ubuntu installer (.deb)](https://github.com/Toji-ue/Prism/releases/download/v0.3.0-beta.1/Prism-0.3.0-beta.1-linux-x86_64-0ce08f79aa5a.deb) | Ubuntu 24.04 desktop, x86_64/amd64 |

Choose the installer for your operating system under **Assets**. GitHub's automatically generated source-code archives are for building Prism from source.

This is a testing release. Physical tablet/artist acceptance, eight-hour recording, clean-machine testing, and the recorded 4000×4000 performance regression remain open. See the [validation ledger](docs/local-candidate-validation.md) for measured results and limits.

## Install

### Windows

Run the Setup executable and follow the installer.

The build does not have a verified publisher signature.

### macOS

Open the DMG and copy **Prism.app together with THIRD-PARTY** to a folder you can write to, such as `~/Applications/Prism/`. Keep the third-party sources and notices beside the app.

This build uses an ad-hoc signature and is **not Apple Developer ID signed or notarized**. macOS may block first launch. Follow your organization's security policy; do not disable Gatekeeper globally.

### Ubuntu 24.04

Download the `.deb`, open a terminal in its download folder, and install it with APT so system dependencies are installed too:

```sh
sudo apt install ./Prism-0.3.0-beta.1-linux-x86_64-*.deb
```

Launch **Prism** from the application menu or run `prism`. Remove the installed application with `sudo apt remove prism`; this package has no scripts that delete your artwork or user recordings.

Other distributions and older glibc versions have not been validated.

### Verify your download

Compare its SHA-256 with the **SHA-256** section in the release notes. Windows provides `Get-FileHash -Algorithm SHA256`; macOS provides `shasum -a 256`; Ubuntu provides `sha256sum`.

## What you can do

- Draw with HB/2B graphite, Ink, and Paint presets; choose or edit brushes in Brush Lab.
- Work with raster layers, nested groups, blend modes, clipping, alpha lock, and undo/redo.
- Save layered artwork as `.prism`, with separate autosave/recovery snapshots.
- Use the full light/dark workspace or switch to Focus mode and quick controls.
- Keep automatic local recordings, review sessions, and export timelapses. Recording is always on and retained until you confirm deletion.
- Export GIF or portable `.prismrec` recordings; **MP4 export currently requires Windows**.

TIFF support is included. TIFF files compressed with JBIG are unsupported in this release. New documents currently use RGB/Alpha, 8-bit, sRGB; wet paint mixing and advanced colour workflows are not implemented.

## First drawing

1. Choose **New Document** or **Open** on the Start Page.
2. Select a brush, colour, and layer; begin drawing.
3. Save your artwork as `.prism`. Autosave/recovery is separate from an explicit project save.
4. Open **REC** to review or export the recording. **F1** opens offline help.

| Shortcut | Action |
| --- | --- |
| `Tab` | Toggle Focus mode from canvas focus |
| `Q` | Quick controls near the cursor |
| `Ctrl+1` | Layers |
| `Ctrl+2` | Choose/edit brushes |
| `Ctrl+3` | Colour |
| `F1` | Offline help |

Panel shortcuts respect text input and active strokes. In the full workspace, Layers stays docked; Focus mode opens it in the shared popover.

## Build from source

Use a C++20 compiler, CMake 3.24+, Ninja, Qt with Quick/Test/Network and image-format plugins, and GoogleTest. The release pipeline uses **Qt 6.11.1**; CMake declares a Qt 6.4 minimum, which is not a separate verified release configuration. Put your compiler and Qt tools on `PATH` before configuring.

On Windows, use the MSYS2 UCRT64 toolchain. Install `mingw-w64-ucrt-x86_64-qt6-imageformats` as well as Qt base/declarative to enable formats such as TIFF and WebP.

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

For a Release build:

```sh
cmake --preset release
cmake --build --preset release
ctest --test-dir build-release --output-on-failure
```

The [CI workflow](.github/workflows/ci.yml) records platform dependencies and native staging. Windows deployment/packaging uses `tools/deploy.ps1` and `tools/package.ps1`; macOS/Linux use the `native-release` preset and `tools/package-native.py`. Packaging requires exact runtime/source reviews and is separate from simply compiling the app.

## Project documentation

- [Roadmap and release gates](docs/roadmap.md)
- [UX contract](docs/ux-beta-plan.md)
- [Candidate validation and known limitations](docs/local-candidate-validation.md)
- [Recording format and compatibility](docs/recording-format.md)
- [Third-party dependencies and notices](third_party/README.md)
- [Native runtime/license review](third_party/native-release-license-review.md)

## License and contributions

Prism's original source code and documentation use the [MIT License](LICENSE). Third-party libraries, sources, and notices retain their own licenses; MIT does not replace those terms. Release packages include third-party notices, corresponding sources, and instructions for replacing LGPL libraries.

Contributions are welcome through issues and pull requests. Describe the change, build and test the affected platforms, and include screenshots for UI changes. Report real tablet testing separately from simulated input. Write project documentation and release notes in English; preserve upstream license texts verbatim.

When reporting an issue, include your OS, Prism version, reproduction steps, and tablet model/driver if relevant. The package's `candidate-manifest.json` identifies the build; exclude private artwork and recordings from reports unless you intend to share them.

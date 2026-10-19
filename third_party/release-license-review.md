# Release license review — 2026-10-18

The requested targets are Windows x64, macOS Apple Silicon and Linux x64.
This engineering review preserves TIFF and the commercial product policy in
AGENTS.md. The pinned Windows review below enables packaging only when its
component/source checks pass. It is not blanket distribution approval. Actual
payload identity and obligations must be established separately for each target.

## TIFF decision and proof

The user approved libtiff without its optional JBIG codec. The pinned version is
4.7.2; [upstream build documentation](https://libtiff.gitlab.io/libtiff/build.html)
describes configurable dependencies. The
[MSYS2 recipe](https://github.com/msys2/MINGW-packages/blob/master/mingw-w64-libtiff/PKGBUILD)
for 4.7.2 enables JBIG and publishes the archive hash used by our build recipe.
The runtime dependency is rebuilt, not replaced with a different codec/library.

`tools/build-tiff-runtime.ps1` builds the shared library with `jbig=OFF`, runs its
tests and rejects PE imports containing JBIG. Windows deployment uses it only
inside its own new stage. Installed toolchain DLLs remain unchanged. Provenance
includes the pinned source archive, exact build recipe and MIT-style license.
Packaging verifies source/license/recipe/runtime hashes rather than accepting an
unmatched custom DLL. The existing GPL and unknown-provenance gates remain.

Initial isolated proof in `build/release-license-review/`:

- Upstream libtiff: **18/18 tests passed**.
- Qt plugin: TIFF None/LZW read/write retains alpha; decoded RGBA pixels are
  identical to the existing libtiff baseline. Both builds exhibit the same
  one-unit premultiplied RGB rounding on the translucent test pattern.
- Deflate RGB TIFF decodes with exact test pixels.
- Loaded DLL path points to the isolated build; its imports and process modules
  contain no JBIG. This proves the tested Windows variant, not other platforms.

TIFF with JBIG compression will be unsupported. Do not imply that omitting the
DLL preserves that optional codec. No commercial JBIG license was acquired.

## Component-level findings

The historical MSYS2 inventory uses package-wide license expressions. A GPL text
in a package can describe tools or an alternative license; it does not establish
that every DLL is GPL-only. Conversely, permissive package metadata alone does
not establish the license of every bundled component. The following findings
come from installed license files retained in each release's `THIRD-PARTY/`
folder. The obsolete generated development snapshot is no longer tracked.
Versions and bytes must be checked against the fresh payload, not copied from
the historical inventory when the toolchain changes.

| Component | Finding | Remaining distribution work |
| --- | --- | --- |
| libtiff 4.7.2 | MIT-style upstream license; optional GPL JBIG removed by approved configuration | Fresh deployment, full dependency closure and TIFF verification |
| liblcms2 | Installed LICENSE is MIT; package-wide GPL expression also covers other content | Preserve exact component notices/provenance; do not ship unreviewed tools |
| libgraphite2 | Installed LICENSE offers MIT as an alternative to LGPL/MPL/GPL | Record chosen MIT terms and preserve notices |
| liblzma | XZ COPYING explicitly identifies liblzma as 0BSD; GPL scripts/tools are separate | Preserve component attribution and identify actual DLL version |
| libzstd | BSD option available in installed dual license | Record BSD selection and keep notice |
| libfreetype | FTL alternative is available | Fulfil FTL attribution and retain notice, including third-party portions |
| libgcc/libstdc++ | Installed README identifies GPL-3.0-or-later with GCC runtime exception | Retain exception and provenance; confirm the build qualifies and no unrelated GCC tools are shipped |
| libpng/libb2/bzip2/libjpeg-turbo | Installed custom/legacy labels require reading actual permissive terms; libjpeg includes IJG terms | Preserve applicable notices/acknowledgements, not just SPDX guesses |
| Qt Core/Gui/Quick/QML/Controls and plugins | Dynamic libraries are replaceable; LGPL route still has obligations and component exceptions | Exact module/third-party inventory, corresponding source/build recipes, replacement instructions and rights notice |
| GLib/libintl/libiconv | LGPL runtime obligations remain | Exact corresponding source, modifications/build recipes, notices and library replacement instructions |
| Other actual runtime DLLs | Must be byte-matched to installed package provenance or the validated custom TIFF build | Regenerate fresh inventory; reject unknown/changed components |

`distribution-review.json` records the reviewed Windows versions, allowed DLL
paths, selected license and hashes of every retained package license file.
The inventory must byte-match installed MSYS2 DLLs or the validated custom TIFF
build. Unknown versions, new component paths and changed license bytes fail the
review lookup. The optional GPL JBIG package remains explicitly blocked.
Selecting an alternative license does not approve unrelated package content.

On 2026-10-18 the owner explicitly approved only `libgcc_s_seh-1.dll` and
`libstdc++-6.dll` under GCC Runtime Library Exception 3.1, with license and notice
copies. This does not authorize JBIG or other GPL components. The exact scope
is recorded in distribution-review.json.

## LGPL work before release

[Qt's published obligations](https://www.qt.io/development/open-source-lgpl-obligations)
require more than dynamic linking. Prepare the exact corresponding library
sources (including modifications), build/installation information, license and
rights notices, and a working replacement mechanism. Supply a source copy or a
valid written offer; a bare upstream URL is not the deliverable. The same release
must preserve the recipient's library-modification/relinking rights. Check
installer/EULA text against those rights and document replacement on each OS.

## Exact Windows source copies

The complete MSYS2 source packages for Qt 6.11.1-1 (base, declarative,
imageformats), GLib 2.88.3-1, gettext 1.0-1 and libiconv 1.19-1 are available
from the official repository. They include upstream source archives, PKGBUILD,
.SRCINFO and local/remote patches. All six detached signatures were verified
against the installed public MSYS2 keyring with FULLY trusted signer
`5F944B027F7FE2091985AA2EFA11531AA0AA7F57`. SHA-256 values for archives and
signatures are pinned in distribution-review.json. An exported public key is
included for independent verification; no private key was read or exported.

Packaging copies these files into THIRD-PARTY/SOURCES (approximately 107 MiB),
checks every hash and refuses an LGPL runtime if its source copy is absent or
changed. It includes NOTICES.txt and LIBRARY-REPLACEMENT.txt. About Prism also
displays the Qt LGPL notice and library-replacement rights. Sources are supplied
with the artifact, so no unsupported future written-offer process is assumed.

The actual deployed Qt DLL allowlist excludes GPL-only tools such as the QML AOT
compiler. Source headers of the included QML debugger plugins were checked in
the exact source archive and offer LGPLv3. Full source archives also contain
build tools/tests under their own terms; supplying those source copies does not
link them into Prism. The GCC runtime exception selection applies to the listed
runtime libraries used by this independent application through normal eligible
GCC compilation, not to bundling compiler tools.

Windows candidate 8deb20e passed fresh no-JBIG deployment, 152 tests, staged
selftest and manifest/ZIP checks, with complete source copies in its artifacts.
Clean-machine installer testing remains open. New toolchain versions require
a new component/source review; this Windows review does not approve another OS.

## Platform status and publication

The workflow builds/tests clients and gates native prerelease packaging.
Release provenance is recorded in GitHub Actions and each package's candidate-manifest.json.
The first actual master run at `ba36462` passed Linux build/tests and failed
macOS compilation on a narrowing alpha conversion. Commit `63694b2` fixes that
conversion; its Windows Release suite passed **152/152**. Subsequent native CI
established the macOS result. The next macOS failure identified unavailable
`std::jthread` in the runner's Apple libc++; the recorder now uses `std::thread`
with its existing explicit stop/join lifecycle. The Windows software-renderer
crash was reproduced locally with QTest/GDB: new canvas tiles were attached
before receiving textures. Tile initialization now precedes attachment; new
tiles also upload when scene-graph state is recreated. The recording interaction
test invokes Focus through real Tab input so viewport preservation uses the same
route as the product. Portable CI now installs qtimageformats and checks
required formats including TIFF, rather than treating a core build as codec proof.

macOS/Linux have separate runtime/SPDX/source reviews, replacement instructions
and native startup checks in `native-release-license-review.md`. Their exact
source/archive checks must pass before Release assets are published. macOS uses
an ad-hoc signature; Developer ID/notarization remains unverified.
Hardware, eight-hour soak, clean-machine, and the recorded performance gate are
separate; a license review does not close them.

The native-release preset stages through Qt's deployment API. CI uploads
prerelease artifacts only after the matching native packaging review passes.
The macOS inventory resolves install names against each image's LC_RPATH and
the application's LC_RPATH, with explicit loader/executable expansion. Missing
bundled files and external non-system libraries fail validation. Unit tests
cover missing @rpath files; native execution remains a separate CI gate.
Linux `ldd` results establish resolution on the runner, not on a clean machine.

Run 39096336188 built and passed all 152 tests on macOS and Linux. macOS passed
the format probe but its staged probe incorrectly requested offscreen while the
bundle selected cocoa. The staged macOS probe now uses its native platform.
Linux initially failed its TIFF probe: the SDK plugin required libtiff.so.5.
Rebuilding that plugin against pinned no-JBIG libtiff 4.7.2 passed in run
39097262425. The current native review records its source and runtime scope.

Windows PS 5.1 attached provider metadata to pacman strings read by Get-Content.
JSON serialization then followed those object graphs and consumed excessive
memory. The package reader now uses plain .NET file lines; the regression test
checks actual parsed metadata remains a small JSON value without provider data.

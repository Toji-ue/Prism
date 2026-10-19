# Native candidate runtime review — 2026-10-18

## macOS arm64

Run 39096872426 built the client on macOS 14 arm64, passed all 152 tests,
loaded TIFF, deployed through Qt's native API and passed the dependency inventory.
The inventory has 85 Mach-O images. Every non-system dependency resolves inside
Prism.app; the remaining dependencies are macOS libraries/frameworks. There are
no GCC, JBIG or external OpenSSL libraries in that payload.

`native-macos-review.json` pins the exact binary path set and the four SDK SPDX
documents from this run. Their runtime components belong to Qt base,
declarative, imageformats and SVG 6.11.1. Qt runtime libraries/plugins select
LGPLv3. GPL-only SDK executables such as moc, rcc, the QML compiler and deployment
tools are build inputs and are excluded from the runtime path set.

The SPDX documents also describe SDK tools and components for other platforms;
they are not a list of everything shipped in Prism. Runtime third-party terms
include MIT/BSD/zlib/libpng, Apache-2.0, Unicode, CC0/public-domain, FTL and IJG.
FreeType selects FTL; D-Bus header fragments select AFL-2.1. Public suffix data
uses MPL-2.0; its unmodified corresponding files and license are in qtbase's
source archive. ICC and PCRE2 terms and the BSD-4-Clause notices for Qt Test's
instrumentation headers are retained with the source/notices. These terms permit
commercial binary distribution subject to their notices; they do not approve
other GPL-only SDK content. Apple frameworks are provided by macOS, not copied.

The Qt imageformats source builds its bundled TIFF from an explicit file list
without JBIG sources/support; the deployed plugin imports only Qt and Apple
frameworks. Source copies for all four modules are hash-pinned from the official
6.11.1 release. They include the third-party code and build system. Packaging
retains the SDK SPDX files, qconfig.pri, readable license/notices and instructions
to rebuild/replace the dynamic frameworks. An ad-hoc signature establishes local
bundle integrity only; it is not Developer ID signing or notarization. Replacing
frameworks remains allowed and re-signing locally is documented.

Native GUI startup and archive checks remain necessary in the packaging run.
Visual review exposed missing theme bindings despite a zero startup exit code:
the embedded `qrc:/Prism` module was outside the default import path. Registering
`qrc:/` fixes it; an isolated Windows fixture reproduced the same failure with
its loose Prism module removed and then passed with the fix. Native Start Page
and drawing startup now reject QML binding errors in addition to saving images.
This is an engineering review of the identified payload, not a blanket license
approval or legal certification. A changed binary path set or SDK SPDX document
requires a fresh review.

## Linux

Run 39097262425 passed all 152 tests, TIFF loading, dependency inventory and
startup from the staged payload. Its original SDK TIFF plugin required
`libtiff.so.5`, which explains why supplying libtiff.so.6 alone did not fix it.
The Qt TIFF target is now rebuilt from the pinned 6.11.1 module source against
the approved libtiff 4.7.2 no-JBIG build. The source archives and recipe accompany
the candidate. Codec development packages retain JPEG, zlib, LZMA, Zstd, Deflate,
LERC and WebP support; the resulting OS runtime dependencies are documented.

`native-linux-review.json` pins the 126 ELF image paths and four SDK SPDX
documents. The payload contains Qt base/declarative/imageformats/SVG, ICU 73.2
and the custom TIFF runtime. GCC, GLib, GTK, D-Bus, Wayland/X11 and graphics
libraries resolve from the OS; no copy of those libraries is in this payload.
Qt's dynamic runtime selects LGPLv3 with the same alternatives/source copies
as macOS. Qt Test's Linux performance UAPI metadata names the Linux syscall
exception; those OS interface declarations do not change Qt Test's LGPL
license. No GPL-only Qt tool/test-utility library is in the runtime set.

The three bundled ICU libraries are pinned by their actual deployed hashes.
The complete ICU 73.2 license, including third-party dictionary notices, was
reviewed from the upstream release tag and is included verbatim. Its permissive
Unicode/ICU terms permit commercial redistribution with notices. Qt's Wayland
code/protocol sources are part of qtbase 6.11.1 and are in the supplied source.
The system desktop dependencies are prerequisites rather than copied payload
components. The candidate targets Ubuntu 24.04 x86_64/glibc 2.39; other Linux
distributions have not been accepted. New paths, SDK versions or ICU hashes
block packaging pending review.

The Ubuntu `.deb` retains the same reviewed runtime, notices, source archives
and replacement instructions under `/opt/prism`. Debian tools are build inputs,
not bundled libraries. The installer adds a launcher and desktop entry; it has
no maintainer scripts that remove user data. `dpkg-shlibdeps` derives OS package
requirements from every ELF, with xcb-cursor added for Qt's dynamic loading.
Packaging verifies the Debian data archive against the staged files, including
source hashes, symlinks and modes. CI install/startup/remove checks run on an
Ubuntu runner and do not replace clean-machine acceptance.

## Acceptance outside this review

Hardware/tablet, an eight-hour recording, clean OS installation, signing and the
open performance gate remain separate. Candidates are prereleases. MP4 encoding
currently uses Windows Media Foundation and is available only on Windows.

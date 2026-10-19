# Runtime dependency provenance and distribution gate

Prism links Qt dynamically and deploys replaceable Qt DLLs. The application is
not a static Qt binary. `tools/deploy.ps1` prepares a fresh runtime directory,
walks PE imports, and verifies image plugins without removing installed formats.
`tools/package.ps1` binds the payload to the tested Release and generates a
versioned candidate manifest, portable ZIP and per-user NSIS installer.

The packaging script generates `THIRD-PARTY/dependency-inventory.json` from
each candidate's actual DLLs, byte-matching against the installed MSYS2 pacman
database. Generated `pacman-desc.txt` files preserve package versions, license
expressions, upstream and packager; installed license texts retain their
package-relative paths. Unmatched DLLs and missing/unreviewed license information
block packaging. These generated snapshots are not committed to the source
repository; the release packages retain their complete notices and sources.

This inventory is evidence, **not a legal-compliance certification**. Package
license expressions can cover tools, documentation and different runtime
components. GPL texts inside Qt's package notice directory do not by themselves
prove that every Qt DLL is GPL-only. Component-level licensing, corresponding
source, build recipes and applicable runtime exceptions require distribution
review. Upstream/recipe URLs identify provenance; they do not substitute for
providing the exact corresponding source where required.

## TIFF/JBIG resolution — approved 2026-10-17

PE imports inspected on the development machine:

`plugins/imageformats/qtiff.dll` -> `libtiff-6.dll` -> `libjbig-0.dll`.

The installed package `mingw-w64-ucrt-x86_64-jbigkit`, version `2.1-5`, declares
`spdx:GPL-2.0`; its installed `COPYING` contains GPL version 2. AGENTS.md requires
explicit sign-off for GPL dependencies that ship. The user chose to retain TIFF
and approved omitting its optional JBIG compression codec. The installed MSYS2
DLLs remain untouched. `tools/build-tiff-runtime.ps1` builds shared libtiff 4.7.2
from its upstream archive with `jbig=OFF`, leaving the other available codecs on.
The source archive SHA-256 is
`672bd7d10aee4606171afb864f3570b83340f6a33e2c186dc0512f7145ffdf6a`, matching
the MSYS2 recipe for this version. libtiff's source license is MIT-style.

Windows deployment substitutes this DLL and rejects remaining JBIG imports.
The payload includes the source archive, LICENSE.md, recipe and build manifest.
Packaging validates their hashes before recognizing the custom TIFF dependency;
an unknown DLL remains blocked. TIFF with JBIG compression is unsupported and
must be disclosed in tester/release notes. TIFF itself is retained. The initial
engineering proof passed 18/18 upstream tests plus Qt TIFF None/LZW/Deflate
checks; decoded None/LZW RGBA pixels matched the installed baseline exactly.

Older deployments may still contain JBIG. Rebuild/deploy before inspecting the
new payload. The obsolete development snapshot has been removed from the source
tree; it remains in Git history. This change does not approve
other dependencies or certify macOS/Linux runtime contents. Detailed findings
and remaining obligations are in [release-license-review.md](release-license-review.md).

## Other distribution review work

Review the exact Qt modules and LGPL linking/replacement/source obligations;
glib/gettext/libiconv LGPL terms; GCC runtime exceptions; and any other package
flagged in the inventory. Supply the matching source/build provenance required
by those licenses. Preserve all relevant copyright and license notices. The
current script deliberately refuses to certify or ship an unreviewed payload.

Historical local runtimes included excess Qt/QML modules. Fresh
`windeployqt --qmldir` deployments use resolved imports;
their new inventory is authoritative for each candidate. No runtime dependencies
were newly installed for this work. Build/test-only GoogleTest is BSD-3-Clause;
MSYS2 GCC/CMake/Ninja and GitHub build actions are development tools, not a claim
that those tool binaries form the application payload.

For the reviewed Windows versions, `distribution-review.json` binds component
selection to exact package versions, DLL paths and installed license hashes.
Packaging includes the complete signed Qt/GLib/libintl/libiconv source packages,
rebuild/replacement instructions and notices, and verifies the pinned source
hashes. Missing sources or changed components keep the gate closed. This is a
specific engineering review, not approval of a new toolchain or another OS.

## Candidate verification and unverified gates

`PrismVersion.txt` contains the one candidate version. The manifest records Git
HEAD, dirty-state/source fingerprint, tested Release executable SHA-256 and every
payload file SHA-256. ZIP entries are hashed after compression; installer staging
is checked before and after NSIS compilation. Installer file SHA-256 is recorded
separately. This verifies staged installer inputs, not a clean-machine install.
The installer deletes only recorded relative application files and never
recursively deletes AppData or unknown user files. Clean Windows
install/upgrade/uninstall, physical tablet and eight-hour drawing acceptance
remain gates requiring recorded results for the exact candidate identity.

Run `powershell -ExecutionPolicy Bypass -File tools/package-validation.ps1 -SelfTest`
for the isolated payload/ZIP/tamper/license fixtures. CI checks Windows synthetic
interactions and Linux/macOS builds/core tests. Reviewed native payloads also
produce source-bearing prerelease artifacts through `tools/package-native.py`.
Platform reviews and limits are in `native-release-license-review.md`; CI
artifacts do not replace external acceptance or a deliberate Release publication.

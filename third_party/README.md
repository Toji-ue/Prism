# Runtime dependency provenance and distribution gate

Prism links Qt dynamically and deploys replaceable Qt DLLs. The application is
not a static Qt binary. `tools/deploy.ps1` prepares a fresh runtime directory,
walks PE imports, and verifies image plugins without removing installed formats.
`tools/package.ps1` binds the payload to the tested Release and generates a
versioned candidate manifest, portable ZIP and per-user NSIS installer.

`notices/dependency-inventory.json` is a snapshot of the development runtime at
the time recorded in that file. Each DLL is byte-matched against files listed
in the installed MSYS2 pacman database. `pacman-desc.txt` preserves package
version, license expression, upstream and packager. Actual installed license
texts are copied with their package-relative paths. The packaging script
regenerates `THIRD-PARTY` from the candidate's actual DLLs; it rejects unmatched
DLLs or missing/review-required license information. Updating the toolchain
therefore cannot silently reuse an old notice bundle.

This inventory is evidence, **not a legal-compliance certification**. Package
license expressions can cover tools, documentation and different runtime
components. GPL texts inside Qt's package notice directory do not by themselves
prove that every Qt DLL is GPL-only. Component-level licensing, corresponding
source, build recipes and applicable runtime exceptions require distribution
review. Upstream/recipe URLs identify provenance; they do not substitute for
providing the exact corresponding source where required.

## Current concrete blocker

PE imports inspected on the development machine:

`plugins/imageformats/qtiff.dll` -> `libtiff-6.dll` -> `libjbig-0.dll`.

The installed package `mingw-w64-ucrt-x86_64-jbigkit`, version `2.1-5`, declares
`spdx:GPL-2.0`; its installed `COPYING` contains GPL version 2. AGENTS.md requires
explicit sign-off for GPL dependencies that ship. **External candidate packaging
fails while this blocker is unresolved.** Development deployment retains TIFF
and this dependency so functionality is not silently changed. No alternative
codec, commercial license, distribution authorization or dependency replacement
has been assumed.

## Other distribution review work

Review the exact Qt modules and LGPL linking/replacement/source obligations;
glib/gettext/libiconv LGPL terms; GCC runtime exceptions; and any other package
flagged in the inventory. Supply the matching source/build provenance required
by those licenses. Preserve all relevant copyright and license notices. The
current script deliberately refuses to certify or ship an unreviewed payload.

The existing local runtime also includes excess Qt/QML modules. A fresh
`windeployqt --qmldir` deployment replaces that broad copy with resolved imports;
its new inventory is authoritative for that candidate. No runtime dependencies
were newly installed for this work. Build/test-only GoogleTest is BSD-3-Clause;
MSYS2 GCC/CMake/Ninja and GitHub build actions are development tools, not a claim
that those tool binaries form the application payload.

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
interactions and Linux/macOS builds/core tests; it does not publish a candidate
or replace the external acceptance gates.

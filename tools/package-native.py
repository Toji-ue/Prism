"""Package only a reviewed, tested native payload with its corresponding sources."""

import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

inventory_spec = importlib.util.spec_from_file_location(
    'native_inventory', Path(__file__).with_name('native-runtime-inventory.py'))
inventory_tool = importlib.util.module_from_spec(inventory_spec)
inventory_spec.loader.exec_module(inventory_tool)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def validate_runtime(payload, inventory, review, evidence):
    payload = payload.resolve(strict=True)
    paths = {p.relative_to(payload).as_posix(): p for p in payload.rglob('*')
             if p.is_file() or p.is_symlink()}
    recorded = [x['path'] for x in inventory['payload']]
    if len(recorded) != len(set(recorded)) or set(recorded) != set(paths):
        raise ValueError('Native inventory file set differs from payload')
    actual = set()
    for relative, path in paths.items():
        if not path.resolve(strict=True).is_relative_to(payload):
            raise ValueError('Native payload link escapes deployment')
        if not path.is_symlink():
            with path.open('rb') as stream:
                if inventory_tool.is_binary(stream.read(4)):
                    actual.add(relative)
    if actual != set(review['runtimePaths']):
        raise ValueError('Native binary paths changed; component review required')
    if inventory['machine'] != review['machine']:
        raise ValueError('Native architecture differs from reviewed payload')
    for row in inventory['payload']:
        path = payload / row['path']
        if 'link' in row:
            if not path.is_symlink() or os.readlink(path) != row['link']:
                raise ValueError('Payload link differs from inventory')
        elif not path.is_file() or digest(path) != row['sha256']:
            raise ValueError('Payload bytes differ from inventory: ' + row['path'])
    for name, expected in review['sdkSbomSha256'].items():
        if digest(evidence / 'sbom' / name) != expected:
            raise ValueError('Qt SDK SPDX changed; native component review required')
    for name, expected in review.get('icu', {}).get('runtimeSha256', {}).items():
        if digest(payload / name) != expected:
            raise ValueError('ICU runtime differs from reviewed SDK copy')


def copy_sources(review, destination, cache):
    cache.mkdir(parents=True, exist_ok=True)
    destination.mkdir(parents=True, exist_ok=True)
    for item in review['sources']:
        name = item['file']
        if not re.fullmatch(r'qt[a-z]+-everywhere-src-6\.11\.1\.tar\.xz', name):
            raise ValueError('Unreviewed Qt source archive name')
        archive = cache / name
        if not archive.exists():
            url = 'https://download.qt.io/official_releases/qt/6.11/6.11.1/submodules/' + name
            with urllib.request.urlopen(url, timeout=120) as source, archive.open('wb') as target:
                shutil.copyfileobj(source, target)
        if digest(archive) != item['sha256']:
            raise ValueError('Corresponding source checksum mismatch: ' + name)
        shutil.copy2(archive, destination / name)
        # Keep notices readable as well as preserving the complete source archive.
        with tarfile.open(archive) as contents:
            for member in contents:
                path = PurePosixPath(member.name)
                if path.is_absolute() or '..' in path.parts:
                    raise ValueError('Invalid source archive path')
                notice = ('LICENSES' in path.parts or path.name == 'qt_attribution.json'
                          or re.match(r'(?i)^(license|licence|copying|copyright|ftl)', path.name))
                if member.isfile() and notice:
                    target = destination.parent / 'QT-NOTICES' / member.name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with contents.extractfile(member) as source, target.open('wb') as output:
                        shutil.copyfileobj(source, output)


def verify_archive(archive, stage):
    expected = {p.relative_to(stage).as_posix(): p for p in stage.rglob('*')
                if p.is_file() or p.is_symlink()}
    seen = set()
    with tarfile.open(archive) as contents:
        for member in contents:
            if member.isdir():
                continue
            name = member.name.removeprefix('./')
            if name in seen or name not in expected:
                raise ValueError('Unexpected or duplicate native archive entry')
            seen.add(name)
            original = expected[name]
            if original.is_symlink():
                if not member.issym() or member.linkname != os.readlink(original):
                    raise ValueError('Native archive link mismatch')
            elif not member.isfile():
                raise ValueError('Unexpected native archive entry type')
            else:
                if member.mode & 0o777 != original.stat().st_mode & 0o777:
                    raise ValueError('Native archive permissions mismatch')
                with contents.extractfile(member) as stream:
                    if hashlib.file_digest(stream, 'sha256').hexdigest() != digest(original):
                        raise ValueError('Native archive content mismatch')
    if seen != set(expected):
        raise ValueError('Native archive missing files')


def build_deb(stage, version, destination):
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9.]+)?', version):
        raise ValueError('Invalid Debian candidate version')
    if destination.exists():
        raise ValueError('Preserve the existing Debian candidate')
    version = version.replace('-', '~', 1)
    with tempfile.TemporaryDirectory(dir=destination.parent) as folder:
        work = Path(folder)
        root = work / 'package'
        application = root / 'opt/prism'
        shutil.copytree(stage, application, symlinks=True)
        (root / 'usr/bin').mkdir(parents=True)
        (root / 'usr/bin/prism').symlink_to('/opt/prism/bin/prism')
        desktop = root / 'usr/share/applications/prism.desktop'
        desktop.parent.mkdir(parents=True)
        desktop.write_text('''[Desktop Entry]
Type=Application
Name=Prism
Comment=Drawing and illustration
Exec=/opt/prism/bin/prism
TryExec=/opt/prism/bin/prism
Terminal=false
Categories=Graphics;2DGraphics;RasterGraphics;
''', encoding='utf-8')
        # Debian derives OS dependencies from every ELF, including Qt plugins.
        # Our reviewed private Qt/ICU/TIFF libraries have no dpkg package metadata.
        (work / 'debian').mkdir()
        (work / 'debian/control').write_text('Source: prism\n\nPackage: prism\nArchitecture: amd64\n')
        command = ['dpkg-shlibdeps', '-O', '--ignore-missing-info', '-l' + str(application / 'lib')]
        for path in sorted(application.rglob('*')):
            if path.is_file() and not path.is_symlink():
                with path.open('rb') as stream:
                    if stream.read(4) == b'\x7fELF':
                        command.append('-e' + str(path))
        result = subprocess.run(command, cwd=work, capture_output=True, text=True)
        if result.returncode:
            raise ValueError('Debian dependency scan failed:\n' + result.stderr)
        dependencies = next((line.removeprefix('shlibs:Depends=') for line in result.stdout.splitlines()
                             if line.startswith('shlibs:Depends=')), '')
        if not dependencies or 'libc6' not in dependencies:
            raise ValueError('Debian system dependencies could not be established')
        # Qt loads xcb-cursor dynamically, so ELF dependency scanning cannot see it.
        dependencies += ', libxcb-cursor0'
        size = sum(p.stat().st_size for p in root.rglob('*') if p.is_file() and not p.is_symlink())
        control = root / 'DEBIAN/control'
        control.parent.mkdir()
        control.write_text(f'''Package: prism
Version: {version}
Architecture: amd64
Maintainer: Toji-ue
Section: graphics
Priority: optional
Installed-Size: {(size + 1023) // 1024}
Depends: {dependencies}
Homepage: https://github.com/Toji-ue/Prism
Description: Prism drawing and illustration prerelease
 Includes the reviewed dynamic runtime and corresponding third-party sources.
 Target: Ubuntu 24.04 amd64. Hardware and clean-machine acceptance remain open.
''', encoding='utf-8')
        subprocess.run(['dpkg-deb', '--root-owner-group', '--build', str(root), str(destination)], check=True)
        actual = subprocess.check_output(['dpkg-deb', '--field', str(destination), 'Version'], text=True).strip()
        if actual != version:
            raise ValueError('Debian candidate metadata differs')
        # Verify the installed data, including sources, permissions and links.
        control.unlink()
        control.parent.rmdir()
        archive = work / 'data.tar'
        with archive.open('wb') as stream:
            subprocess.run(['dpkg-deb', '--fsys-tarfile', str(destination)], stdout=stream, check=True)
        verify_archive(archive, root)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('payload', type=Path)
    parser.add_argument('inventory', type=Path)
    parser.add_argument('evidence', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    target = {'Darwin': 'macos', 'Linux': 'linux'}.get(platform.system())
    if target is None:
        parser.error('Use the Windows PowerShell packaging gate on Windows')
    review_path = root / f'third_party/native-{target}-review.json'
    review = json.loads(review_path.read_text())  # Missing review blocks packaging.
    if review['schemaVersion'] != 1 or review['platform'] != platform.system():
        raise ValueError('Unsupported native component review')
    inventory = json.loads(args.inventory.read_text())
    payload = args.payload.resolve(strict=True)
    validate_runtime(payload, inventory, review, args.evidence)
    status = subprocess.check_output(['git', 'status', '--porcelain'], cwd=root, text=True).strip()
    if status:
        raise ValueError('Commit the native candidate source before packaging:\n' + status)
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
    version = (root / 'PrismVersion.txt').read_text().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?', version):
        raise ValueError('Invalid candidate version')
    name = f'Prism-{version}-{target}-{review["machine"]}-{commit[:12]}'
    output = root / 'build/native-candidates' / name
    output.mkdir(parents=True)  # Preserve earlier artifacts; never overwrite a candidate.
    stage = output / 'stage'
    shutil.copytree(payload, stage, symlinks=True)
    notices = stage / 'THIRD-PARTY'
    notices.mkdir()
    copy_sources(review, notices / 'SOURCES', root / 'build/native-sources')
    shutil.copy2(review_path, notices / review_path.name)
    shutil.copy2(root / 'third_party/native-release-license-review.md', notices)
    shutil.copytree(args.evidence, notices / 'SDK-EVIDENCE')
    if target == 'linux':
        icu_license = root / 'third_party' / review['icu']['licenseFile']
        if digest(icu_license) != review['icu']['licenseSha256']:
            raise ValueError('ICU license differs from reviewed notice')
        shutil.copy2(icu_license, notices)
        tiff = root / 'build/native-tiff'
        if digest(tiff / 'tiff-4.7.2.tar.gz') != review['tiff']['sourceSha256']:
            raise ValueError('Native TIFF source mismatch')
        tiff_notice = notices / 'TIFF'
        tiff_notice.mkdir()
        shutil.copy2(tiff / 'tiff-4.7.2.tar.gz', tiff_notice)
        shutil.copy2(tiff / 'tiff-4.7.2/LICENSE.md', tiff_notice)
        shutil.copy2(root / 'tools/build-native-tiff.py', tiff_notice)
        replacement = '''Quit Prism, back up the folder and replace matching .so libraries in lib/
and plugins in plugins/ or qml/. Run bin/prism; its relative runtime paths load
the replaceable libraries. No candidate hashes are enforced at startup.
TIFF's no-JBIG source and exact plugin/runtime build recipe are in THIRD-PARTY/TIFF.
ICU 73.2 uses the included permissive Unicode/ICU notices; its exact SDK copies
are identified in the review and runtime manifest.'''
        target_notice = '''Linux x86_64 candidate built/tested on Ubuntu 24.04 (glibc 2.39). Extract
the entire archive to a writable folder and run bin/prism. Retain THIRD-PARTY.
System graphics/desktop libraries remain OS dependencies. On Ubuntu use the
usual desktop runtime plus libxcb-cursor0, libxkbcommon-x11-0, libjpeg-turbo8,
liblzma5, libzstd1, libdeflate0, liblerc4, libwebp7 and zlib1g. Other distributions
and older glibc versions have not been validated. No clean-machine acceptance
or universal Linux compatibility is claimed.'''
    else:
        replacement = '''Quit Prism, back up the app and replace matching frameworks/plugins in
Prism.app/Contents/Frameworks and PlugIns. On macOS an ad-hoc local signature
can be regenerated with: codesign --force --deep --sign - Prism.app
Prism does not enforce candidate hashes at startup; changed libraries remain
replaceable. A local signature is not Apple Developer ID or notarization.'''
        target_notice = '''This macOS arm64 candidate is unsigned by a verified publisher and is not
notarized. Copy Prism.app to a writable application folder. Retain THIRD-PARTY
beside it. The build was tested on macOS 14; no clean-machine acceptance is
claimed.'''
    instructions = f'''PRISM NATIVE PRERELEASE CANDIDATE

Qt 6.11.1 libraries/plugins are dynamically linked under LGPLv3. Complete
corresponding sources are in THIRD-PARTY/SOURCES; build flags/SPDX documents
are in SDK-EVIDENCE and readable notices in QT-NOTICES. Third-party terms and
alternative selections are recorded in the accompanying component review.
Portions are copyright 1996-2026 The FreeType Project (https://freetype.org).
This software is based in part on the work of the Independent JPEG Group.

You may rebuild/replace LGPL libraries and reverse engineer Prism as necessary
to debug those modifications. No Prism terms restrict these library rights.
Extract each Qt module source archive. Configure qtbase with its configure
script using -release -opensource -confirm-license -shared -nomake examples
-nomake tests -prefix <new-prefix>, then build/install with CMake. Configure the
remaining modules with <new-prefix>/bin/qt-configure-module and build/install.
Keep version/architecture compatible with this candidate and use the SDK build
flags in qconfig.pri for features your modification needs.

{replacement}

{target_notice}
Qt's sources include their own third-party notices and build recipes.

Recording is automatic and retained until confirmed deletion. TIFF is included;
JBIG compression is unsupported. MP4 export currently requires Windows.
Hardware/tablet, three-artist UX, eight-hour recording, clean OS installation
and the open performance gate remain pending. This is not production-ready.
Include candidate-manifest.json with a bug report; preserve private artwork.
'''
    (stage / 'README-TESTER.txt').write_text(instructions, encoding='utf-8')
    manifest = {'schemaVersion': 1, 'version': version, 'gitCommit': commit,
                'createdUtc': datetime.now(timezone.utc).isoformat(), 'platform': target,
                'machine': review['machine'], 'runtimeInventory': inventory,
                'distributionLicenseStatus': 'Exact native component/SPDX review matched; corresponding sources and notices included; not a legal certification',
                'acceptance': 'prerelease; hardware, eight-hour, clean-machine and performance gates open'}
    (stage / 'candidate-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    archive = output / (name + '.tar.gz')
    with tarfile.open(archive, 'w:gz', dereference=False) as contents:
        for path in sorted(stage.iterdir()):
            contents.add(path, arcname=path.name)
    verify_archive(archive, stage)
    artifacts = [{'path': archive.name, 'sha256': digest(archive)}]
    if target == 'macos':
        dmg = output / (name + '.dmg')
        subprocess.run(['hdiutil', 'create', '-volname', 'Prism', '-srcfolder', str(stage),
                        '-format', 'UDZO', str(dmg)], check=True)
        subprocess.run(['hdiutil', 'verify', str(dmg)], check=True)
        artifacts.append({'path': dmg.name, 'sha256': digest(dmg)})
    else:
        deb = output / (name + '.deb')
        build_deb(stage, version, deb)
        artifacts.append({'path': deb.name, 'sha256': digest(deb)})
    if commit != subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip():
        raise ValueError('Source commit changed during native packaging')
    (output / 'artifacts-manifest.json').write_text(json.dumps({
        'candidate': manifest, 'artifacts': artifacts,
        'verified': ['reviewed binary paths and SDK SPDX', 'runtime payload hashes',
                     'corresponding source hashes', 'every tar entry', 'DMG verification if applicable',
                     'Debian metadata and every installed file if applicable']}, indent=2) + '\n')
    print('Verified native prerelease:', output)


if __name__ == '__main__':
    main()

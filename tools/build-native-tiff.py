"""Build the approved TIFF variant for the Linux Qt image plugin, without JBIG."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import urllib.request

root = Path(__file__).resolve().parents[1]
work = root / 'build/native-tiff'
work.mkdir(parents=True, exist_ok=True)
archive = work / 'tiff-4.7.2.tar.gz'
expected = '672bd7d10aee4606171afb864f3570b83340f6a33e2c186dc0512f7145ffdf6a'
if not archive.exists():
    with urllib.request.urlopen('https://download.osgeo.org/libtiff/tiff-4.7.2.tar.gz', timeout=120) as source, archive.open('wb') as target:
        shutil.copyfileobj(source, target)
with archive.open('rb') as source:
    if hashlib.file_digest(source, 'sha256').hexdigest() != expected:
        raise ValueError('TIFF source checksum mismatch')
subprocess.run(['cmake', '-E', 'tar', 'xzf', str(archive)], cwd=work, check=True)
source = work / 'tiff-4.7.2'
build = work / 'build'
prefix = work / 'prefix'
subprocess.run(['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_SHARED_LIBS=ON', '-Djbig=OFF', '-Dtiff-tools=OFF', '-Dtiff-tests=ON', '-Dtiff-contrib=OFF', '-Dtiff-docs=OFF', '-DCMAKE_INSTALL_LIBDIR=lib', '-DCMAKE_INSTALL_PREFIX=' + str(prefix)], check=True)
subprocess.run(['cmake', '--build', str(build), '--parallel', '2'], check=True)
subprocess.run(['ctest', '--test-dir', str(build), '--output-on-failure'], check=True)
subprocess.run(['cmake', '--install', str(build)], check=True)
if not (prefix / 'lib/libtiff.so.6').is_file():
    raise ValueError('Expected shared libtiff.so.6 was not built')
for library in (prefix / 'lib').glob('libtiff.so*'):
    deps = subprocess.run(['ldd', str(library)], text=True, capture_output=True, check=True).stdout
    if 'jbig' in deps.lower() or 'not found' in deps:
        raise ValueError('Unapproved or missing TIFF dependency: ' + deps)
print('Verified TIFF prefix:', prefix)

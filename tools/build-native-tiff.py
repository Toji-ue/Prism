"""Build the approved TIFF variant for the Linux Qt image plugin, without JBIG."""
import hashlib
import json
import os
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

# The SDK's image plugin is a separate build product. Build its TIFF target
# against this exact runtime instead of relying on a vendor plugin's ABI.
qt = Path(os.environ['QT_ROOT_DIR'])
plugin_archive = work / 'qtimageformats-everywhere-src-6.11.1.tar.xz'
plugin_hash = 'b2bf6c6845ac175ed7f819145483ba4676f617aaa6a5012c8efee63c8bbac413'
if not plugin_archive.exists():
    with urllib.request.urlopen('https://download.qt.io/official_releases/qt/6.11/6.11.1/submodules/' + plugin_archive.name, timeout=120) as source_stream, plugin_archive.open('wb') as target:
        shutil.copyfileobj(source_stream, target)
with plugin_archive.open('rb') as source_stream:
    if hashlib.file_digest(source_stream, 'sha256').hexdigest() != plugin_hash:
        raise ValueError('Qt imageformats source checksum mismatch')
subprocess.run(['cmake', '-E', 'tar', 'xJf', str(plugin_archive)], cwd=work, check=True)
plugin_source = work / 'qtimageformats-everywhere-src-6.11.1'
plugin_build = work / 'qtimageformats-build'
subprocess.run(['cmake', '-S', str(plugin_source), '-G', 'Ninja',
                '-B', str(plugin_build), '-DCMAKE_BUILD_TYPE=Release',
                '-DQT_BUILD_TESTS=OFF', '-DQT_BUILD_EXAMPLES=OFF', '-DINPUT_tiff=system',
                '-DTIFF_ROOT=' + str(prefix), '-DCMAKE_PREFIX_PATH=' + str(qt) + ';' + str(prefix)], check=True)
subprocess.run(['cmake', '--build', str(plugin_build), '--target', 'QTiffPlugin', '--parallel', '2'], check=True)
plugin = plugin_build / 'plugins/imageformats/libqtiff.so'
if not plugin.is_file():
    raise ValueError('Expected Qt TIFF plugin was not built')
shutil.copy2(plugin, qt / 'plugins/imageformats/libqtiff.so')

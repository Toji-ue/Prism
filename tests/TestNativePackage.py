import importlib.util
from pathlib import Path
import shutil
import tarfile
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    'native_package', Path(__file__).resolve().parents[1] / 'tools/package-native.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class NativePackageTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which('dpkg-deb') and shutil.which('dpkg-shlibdeps'),
                         'Debian package tools required')
    def test_deb_retains_sources_links_and_executable(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            stage = root / 'stage'
            (stage / 'bin').mkdir(parents=True)
            shutil.copy2('/usr/bin/true', stage / 'bin/prism')
            (stage / 'THIRD-PARTY/SOURCES').mkdir(parents=True)
            (stage / 'THIRD-PARTY/SOURCES/notice.txt').write_text('retained source notice')
            (stage / 'bin/alias').symlink_to('prism')
            candidate = root / 'prism.deb'
            package.build_deb(stage, '0.3.0-beta.1', candidate)
            self.assertTrue(candidate.is_file())
            with self.assertRaisesRegex(ValueError, 'existing Debian candidate'):
                package.build_deb(stage, '0.3.0-beta.1', candidate)

    def test_deb_rejects_invalid_version(self):
        with self.assertRaisesRegex(ValueError, 'Invalid Debian candidate version'):
            package.build_deb(Path('.'), '1.0\nDepends: injected', Path('unused.deb'))

    def test_payload_and_sdk_changes_block_review(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'sbom').mkdir()
            binary = root / 'Prism'
            binary.write_bytes(b'\xcf\xfa\xed\xfereviewed program')
            sbom = root / 'sbom/qtbase.json'
            sbom.write_bytes(b'reviewed SDK')
            review = {'runtimePaths': ['Prism'], 'machine': 'arm64',
                      'sdkSbomSha256': {'qtbase.json': package.digest(sbom)}}
            inventory = {'machine': 'arm64', 'payload': [
                {'path': 'Prism', 'sha256': package.digest(binary), 'dependencies': []},
                {'path': 'sbom/qtbase.json', 'sha256': package.digest(sbom)}]}
            package.validate_runtime(root, inventory, review, root)
            binary.write_bytes(b'\xcf\xfa\xed\xfechanged program')
            with self.assertRaisesRegex(ValueError, 'Payload bytes differ'):
                package.validate_runtime(root, inventory, review, root)
            inventory['payload'][0]['sha256'] = package.digest(binary)
            sbom.write_bytes(b'changed SDK')
            inventory['payload'][1]['sha256'] = package.digest(sbom)
            with self.assertRaisesRegex(ValueError, 'SDK SPDX changed'):
                package.validate_runtime(root, inventory, review, root)
            (root / 'unreviewed').write_bytes(b'\xcf\xfa\xed\xfecode')
            with self.assertRaisesRegex(ValueError, 'file set differs'):
                package.validate_runtime(root, inventory, review, root)
            inventory['payload'].append({'path': 'unreviewed', 'sha256': package.digest(root / 'unreviewed')})
            with self.assertRaisesRegex(ValueError, 'binary paths changed'):
                package.validate_runtime(root, inventory, review, root)

    def test_archive_checks_bytes_and_extra_entries(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            stage = root / 'stage'
            stage.mkdir()
            binary = stage / 'Prism'
            binary.write_bytes(b'candidate')
            archive = root / 'candidate.tar'
            with tarfile.open(archive, 'w') as output:
                output.add(binary, arcname='Prism')
            package.verify_archive(archive, stage)
            binary.write_bytes(b'tampered')
            with self.assertRaisesRegex(ValueError, 'content mismatch'):
                package.verify_archive(archive, stage)
            binary.write_bytes(b'candidate')
            with tarfile.open(archive, 'a') as output:
                output.add(binary, arcname='../escape')
            with self.assertRaises(ValueError):
                package.verify_archive(archive, stage)


if __name__ == '__main__':
    unittest.main()

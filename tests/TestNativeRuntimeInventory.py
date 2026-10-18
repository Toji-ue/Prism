import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "native_inventory", Path(__file__).resolve().parents[1] / "tools/native-runtime-inventory.py")
inventory = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inventory)


class NativeRuntimeInventoryTest(unittest.TestCase):
    def test_macos_rpath_requires_a_real_bundled_library(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            executable = root / 'Prism.app/Contents/MacOS/Prism'
            executable.parent.mkdir(parents=True)
            executable.write_bytes(b'program')
            library = root / 'Prism.app/Contents/Frameworks/QtCore.framework/Versions/A/QtCore'
            library.parent.mkdir(parents=True)
            library.write_bytes(b'library')
            commands = 'cmd LC_RPATH\ncmdsize 48\npath @executable_path/../Frameworks (offset 12)'
            dependencies = 'Prism:\n\t@rpath/QtCore.framework/Versions/A/QtCore (compatibility version 6.0.0, current version 6.11.1)'
            with patch.object(inventory.subprocess, 'run', return_value=
                              inventory.subprocess.CompletedProcess([], 0, commands, '')):
                result = inventory.resolve_macho_dependencies(executable, executable, root, dependencies, {})
                self.assertEqual(result[0]['path'], library.relative_to(root).as_posix())
                library.unlink()
                with self.assertRaisesRegex(ValueError, 'Unresolved bundled macOS dependency'):
                    inventory.resolve_macho_dependencies(executable, executable, root, dependencies, {})

    def test_source_text_is_hashed_without_executing_dependency_tools(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            (root / "README.txt").write_text("runtime information")
            with patch.object(inventory.platform, "system", return_value="Linux"), \
                 patch.object(inventory.subprocess, "run") as run:
                result = inventory.inspect(root)
            self.assertEqual(result[0]["path"], "README.txt")
            self.assertEqual(len(result[0]["sha256"]), 64)
            run.assert_not_called()

    def test_missing_dependency_and_jbig_are_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            (root / "prism").write_bytes(b"\x7fELFtest")
            for dependencies in ("libQt6Core.so => not found", "libjbig.so => /lib/libjbig.so"):
                with self.subTest(dependencies=dependencies), \
                     patch.object(inventory.platform, "system", return_value="Linux"), \
                     patch.object(inventory.subprocess, "run", return_value=
                                  inventory.subprocess.CompletedProcess([], 0, dependencies, "")):
                    with self.assertRaises(ValueError):
                        inventory.inspect(root)

    def test_sdk_dependency_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            (root / "Prism").write_bytes(b"\xcf\xfa\xed\xfetest")
            with patch.object(inventory.platform, "system", return_value="Darwin"), \
                 patch.dict(os.environ, {"QT_ROOT_DIR": "/qt/sdk"}), \
                 patch.object(inventory.subprocess, "run", return_value=
                              inventory.subprocess.CompletedProcess([], 0, "/qt/sdk/QtCore", "")):
                with self.assertRaisesRegex(ValueError, "still references Qt SDK"):
                    inventory.inspect(root)

    def test_symlink_cannot_escape_payload(self):
        with tempfile.TemporaryDirectory() as folder:
            base = Path(folder).resolve()
            root = base / "payload"
            root.mkdir()
            outside = base / "outside"
            outside.write_text("outside deployment")
            try:
                (root / "link").symlink_to(outside)
            except OSError as error:
                self.skipTest(f"Symlink creation unavailable: {error}")
            with patch.object(inventory.platform, "system", return_value="Linux"):
                with self.assertRaisesRegex(ValueError, "symlink escapes payload"):
                    inventory.inspect(root)


if __name__ == "__main__":
    unittest.main()

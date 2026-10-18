"""Inspect a native deployment for review; this does not authorize publication."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess


def is_binary(header):
    return header in {
        b"\x7fELF", b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe",
        b"\xfe\xed\xfa\xcf", b"\xfe\xed\xfa\xce", b"\xca\xfe\xba\xbe",
        b"\xbe\xba\xfe\xca", b"\xca\xfe\xba\xbf", b"\xbf\xba\xfe\xca",
    }


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def expand_macho_path(value, binary, executable):
    for token, base in (('@loader_path', binary.parent), ('@executable_path', executable.parent)):
        if value == token or value.startswith(token + '/'):
            return base / value[len(token):].lstrip('/')
    return Path(value) if value.startswith('/') else None

def macho_commands(binary, environment):
    output = subprocess.run(['otool','-l',str(binary)], capture_output=True, text=True, env=environment, check=True).stdout
    rpaths = re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.+?) \(offset \d+\)', output)
    names = re.findall(r'cmd LC_ID_DYLIB\s+cmdsize \d+\s+name (.+?) \(offset \d+\)', output)
    return rpaths, names

def resolve_macho_dependencies(binary, executable, root, dependencies, environment):
    search = []
    own_names = []
    for owner in dict.fromkeys((binary, executable)):
        rpaths, names = macho_commands(owner, environment)
        if owner == binary:
            own_names = names
        for value in rpaths:
            folder = expand_macho_path(value, owner, executable)
            if folder is not None:
                search.append(folder)
    resolved = []
    for line in dependencies.splitlines():
        if ' (compatibility version ' not in line:
            continue
        name = line.strip().split(' (compatibility version ',1)[0]
        if name in own_names:
            continue
        if name.startswith(('/System/Library/', '/usr/lib/')):
            resolved.append({'name':name,'provider':'macOS'})
            continue
        if name.startswith('@rpath/'):
            candidates = [folder / name[len('@rpath/'):] for folder in search]
        else:
            direct = expand_macho_path(name, binary, executable)
            candidates = [direct] if direct else []
        actual = next((p.resolve() for p in candidates if p.is_file() and p.resolve().is_relative_to(root)), None)
        if actual is None:
            raise ValueError(f'Unresolved bundled macOS dependency in {binary.relative_to(root)}: {name}')
        resolved.append({'name':name,'path':actual.relative_to(root).as_posix()})
    return resolved


def inspect(root):
    system = platform.system()
    if system not in {"Darwin", "Linux"}:
        raise ValueError("Native runtime review requires macOS or Linux")
    environment = os.environ.copy()
    for name in ("LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH", "QT_PLUGIN_PATH"):
        environment.pop(name, None)
    result = []
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root).as_posix()
        if path.is_symlink():
            if not path.resolve(strict=True).is_relative_to(root):
                raise ValueError(f"Deployment symlink escapes payload: {relative}")
            result.append({"path": relative, "link": os.readlink(path)})
            continue
        if not path.is_file():
            continue
        entry = {"path": relative, "bytes": path.stat().st_size, "sha256": digest(path)}
        with path.open("rb") as stream:
            binary = is_binary(stream.read(4))
        if binary:
            command = ["otool", "-L", str(path)] if system == "Darwin" else ["ldd", str(path)]
            process = subprocess.run(command, capture_output=True, text=True, env=environment)
            dependencies = process.stdout.strip()
            if process.returncode or "not found" in dependencies:
                raise ValueError(f"Unresolved native dependencies for {relative}: {dependencies}")
            if "jbig" in dependencies.lower():
                raise ValueError(f"Unapproved JBIG dependency in {relative}")
            qt_root = os.environ.get("QT_ROOT_DIR", "")
            if qt_root and qt_root in dependencies:
                raise ValueError(f"Deployment still references Qt SDK for {relative}")
            entry["dependencies"] = dependencies.splitlines()
            if system == "Darwin":
                executables = list(root.glob("*.app/Contents/MacOS/Prism"))
                if len(executables) != 1:
                    raise ValueError("Expected one Prism.app executable in native payload")
                entry["resolvedDependencies"] = resolve_macho_dependencies(
                    path, executables[0], root, dependencies, environment)
        result.append(entry)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("payload", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    root = args.payload.resolve(strict=True)
    if not root.is_dir() or args.output.resolve().is_relative_to(root):
        parser.error("Use an existing payload directory and a report outside that directory")
    data = {"schemaVersion": 1, "platform": platform.platform(), "machine": platform.machine(),
            "distributionReview": "pending native component/source review",
            "dependencyResolution": ("bundled install names resolved with LC_RPATH; macOS system libraries supplied by OS"
                                     if platform.system() == "Darwin" else "ldd resolved on build host; clean-machine acceptance pending"),
            "payload": inspect(root)}
    args.output.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(f"Recorded {len(data['payload'])} entries; native license review still required")


if __name__ == "__main__":
    main()

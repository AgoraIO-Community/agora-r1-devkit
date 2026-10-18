#!/usr/bin/env python3
"""Verify official BK sources and the preserved MyBot/AOSL/RTSA snapshots."""

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

from vendor_workspace import ROOT, load_lock, validate_sources


MANAGED = (
    "mybot_sdk/include", "mybot_sdk/src", "mybot_sdk/platforms",
    "mybot_aosl/aosl", "mybot_rtsa/include", "mybot_rtsa/lib", "mybot_rtsa/hal",
)
GENERATED = {"build", "__pycache__", ".git"}


def check_snapshots(root):
    product = Path(root) / "components/mybot"
    lock = product / "snapshots.json"
    if lock.is_symlink() or not lock.is_file():
        raise RuntimeError(f"missing product snapshot manifest: {lock}")
    manifest = json.loads(lock.read_text(encoding="utf-8"))
    files = manifest.get("files")
    if manifest.get("schema_version") != 1 or not isinstance(files, dict) or not files:
        raise RuntimeError(f"unsupported product snapshot manifest: {lock}")
    for name, expected in files.items():
        relative = Path(name)
        if relative.is_absolute() or ".." in relative.parts or not relative.parts or \
                relative.parts[0] not in {"mybot_sdk", "mybot_aosl", "mybot_rtsa"}:
            raise RuntimeError(f"invalid product snapshot path: {name}")
        if not isinstance(expected, str) or len(expected) != 64 or \
                any(character not in "0123456789abcdef" for character in expected):
            raise RuntimeError(f"invalid product snapshot checksum: {name}")
        path = product / relative
        if any((product / Path(*relative.parts[:index])).is_symlink()
               for index in range(1, len(relative.parts) + 1)) or not path.is_file():
            raise RuntimeError(f"missing or linked product snapshot: {path}")
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            raise RuntimeError(f"product snapshot checksum mismatch: {name}")
    for directory in MANAGED:
        base = product / directory
        if base.is_symlink() or not base.is_dir():
            raise RuntimeError(f"missing product snapshot directory: {base}")
        for path in base.rglob("*"):
            relative = path.relative_to(product)
            if GENERATED.intersection(relative.parts) or path.suffix == ".pyc":
                continue
            if path.is_symlink():
                raise RuntimeError(f"linked product snapshot input: {relative}")
            if path.is_file() and relative.as_posix() not in files:
                raise RuntimeError(f"unrecorded product snapshot input: {relative}")
    print(f"PASS preserved MyBot/AOSL/RTSA snapshots: {len(files)} files")
    return len(files)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    root = parser.parse_args(argv).root.resolve()
    try:
        manifest, _ = load_lock(root)
        validate_sources(root, manifest)
        for name, repository in manifest["repositories"].items():
            print(f"PASS official {name}: {repository['commit']} (tracked tree clean)")
        check_snapshots(root)
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("Source checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Prepare an isolated MyBot project and patched official BK sources for make."""

import argparse
import shlex
import subprocess
import sys
from pathlib import Path

import build_all
from vendor_workspace import prepare_vendor


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=Path, default=build_all.ROOT / "build")
    parser.add_argument("--language", choices=build_all.LANGUAGES, default="zh-CN")
    args = parser.parse_args()
    build_root = args.build_root.expanduser().resolve()
    if build_all.ROOT.is_relative_to(build_root) or \
            (build_root.is_relative_to(build_all.ROOT) and
             not build_root.is_relative_to(build_all.ROOT / "build")):
        raise RuntimeError("build root must be outside the repository or under its build directory")
    lines, found, options = build_all.source_config()
    build_all.check_product_sources()
    stage = prepare_vendor(build_root)
    name = build_all.firmware_name(args.language)
    project = build_all.prepare_variant(
        build_root / f"build-{name}",
        build_all.variant_config(lines, found, options, args.language), clean=False,
        components=stage / "bk_solution_ai/components")
    print("Prepared official BK dependencies and MyBot project. Build with:")
    print(shlex.join(["make", "-C", str(project), "bk7258",
                      f"SDK_DIR={stage / 'bk_avdk_smp'}"]))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        sys.exit(1)

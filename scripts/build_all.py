#!/usr/bin/env python3
"""Build BK7258 firmware for Chinese and English without changing the source project."""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import time
from contextlib import nullcontext
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
PROJECT = ROOT / "bk_solution_ai/projects/mybot"
COMPONENTS = ROOT / "bk_solution_ai/components"
SDK = ROOT / "bk_avdk_smp"
LANGUAGES = ("zh-CN", "en-US")
MARKER = "mybot-bk7258-build-all-v1\n"
ENDPOINTS = {
    "zh-CN": b"https://botstation.shengwang.cn/api",
    "en-US": b"https://botstation.agora.io/api",
}


def elapsed(seconds):
    total = max(0, int(seconds))
    hours, remainder = divmod(total, 3600)
    minutes, seconds = divmod(remainder, 60)
    return f"{hours:02d}:{minutes:02d}:{seconds:02d}"


def source_config():
    path = PROJECT / "ap/config/bk7258_ap/config"
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    options = {
        "zh-CN": "CONFIG_MYBOT_LANGUAGE_ZH_CN",
        "en-US": "CONFIG_MYBOT_LANGUAGE_EN_US",
    }
    found = {language: [] for language in LANGUAGES}
    for index, line in enumerate(lines):
        value = line.rstrip("\r\n")
        for language, symbol in options.items():
            if value in (f"{symbol}=y", f"# {symbol} is not set"):
                found[language].append(index)
    if any(len(indices) != 1 for indices in found.values()):
        raise RuntimeError(f"expected one setting for each language in {path}")
    active = [language for language, symbol in options.items()
              if lines[found[language][0]].rstrip("\r\n") == f"{symbol}=y"]
    if len(active) != 1:
        raise RuntimeError(f"expected exactly one active language in {path}")
    return lines, found, options


def select_python():
    candidates = [shutil.which("python3"), "/usr/bin/python3"]
    for candidate in dict.fromkeys(candidates):
        if not candidate or not Path(candidate).is_file():
            continue
        check = subprocess.run(
            [candidate, "-c", "import click_option_group"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            check=False,
        )
        if check.returncode == 0:
            return Path(candidate).resolve()
    raise RuntimeError("Armino Python needs click_option_group; install its build dependencies")


def variant_config(lines, found, options, language):
    configured = lines.copy()
    for choice, symbol in options.items():
        index = found[choice][0]
        newline = "\r\n" if configured[index].endswith("\r\n") else (
            "\n" if configured[index].endswith("\n") else "")
        value = f"{symbol}=y" if choice == language else f"# {symbol} is not set"
        configured[index] = value + newline
    return "".join(configured)


def prepare_variant(path, config, clean):
    marker = path / ".mybot-build-all"
    if path.exists() or path.is_symlink():
        if path.is_symlink() or not path.is_dir() or not marker.is_file() or \
                marker.read_text(encoding="ascii") != MARKER:
            raise RuntimeError(f"refusing to replace a build directory not owned by this script: {path}")
        if clean:
            print(f"Cleaning build: {path}", flush=True)
            shutil.rmtree(path)
    path.mkdir(parents=True, exist_ok=True)
    marker.write_text(MARKER, encoding="ascii")

    solution = path / "solution"
    solution.mkdir(exist_ok=True)
    components_link = solution / "components"
    if not components_link.exists() and not components_link.is_symlink():
        components_link.symlink_to(COMPONENTS, target_is_directory=True)
    elif not components_link.is_symlink() or components_link.resolve() != COMPONENTS:
        raise RuntimeError(f"unexpected component link: {components_link}")

    copy = solution / "projects/mybot"
    if copy.exists() or copy.is_symlink():
        if copy.is_symlink() or not copy.is_dir():
            raise RuntimeError(f"unexpected project copy: {copy}")
        build = copy / "build"
        if build.is_symlink() or (build.exists() and not build.is_dir()):
            raise RuntimeError(f"unexpected project build directory: {build}")
        for entry in copy.iterdir():
            if entry.name == "build":
                continue
            if entry.is_dir() and not entry.is_symlink():
                shutil.rmtree(entry)
            else:
                entry.unlink()
    shutil.copytree(PROJECT, copy, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns("build", "releases", "__pycache__", "*.pyc"))
    (copy / "ap/config/bk7258_ap/config").write_text(config, encoding="utf-8")
    return copy


def check_firmware(project, language):
    built = project / "build/bk7258/mybot"
    image = built / "package/all-app.bin"
    ap_config = built / "bk7258_ap/config/sdkconfig.h"
    ap_elf = built / "bk7258_ap/app.elf"
    if not image.is_file() or not image.stat().st_size or not ap_config.is_file() or \
            not ap_elf.is_file():
        raise RuntimeError(f"missing AP/CP image, AP configuration, or ELF under {built}")

    config = ap_config.read_text(encoding="utf-8")
    selected = "ZH_CN" if language == "zh-CN" else "EN_US"
    other = "EN_US" if language == "zh-CN" else "ZH_CN"
    if f"#define CONFIG_MYBOT_LANGUAGE_{selected} 1" not in config or \
            f"#define CONFIG_MYBOT_LANGUAGE_{other} 1" in config:
        raise RuntimeError(f"generated AP configuration does not match {language}")
    elf = ap_elf.read_bytes()
    if ENDPOINTS[language] not in elf or ENDPOINTS["en-US" if language == "zh-CN" else "zh-CN"] in elf:
        raise RuntimeError(f"AP firmware has the wrong device-service endpoint for {language}")
    return image


def publish_outputs(staged, outputs):
    backups = {}
    for output in outputs:
        if output.is_symlink() or (output.exists() and not output.is_file()):
            raise RuntimeError(f"firmware destination is not a regular file: {output}")
        if output.exists():
            backup = Path(staged) / f".{output.name}.previous"
            shutil.copy2(output, backup)
            backups[output] = backup

    published = []
    try:
        for output in outputs:
            os.replace(Path(staged) / output.name, output)
            published.append(output)
    except OSError as error:
        rollback_errors = []
        for output in reversed(published):
            try:
                if output in backups:
                    os.replace(backups[output], output)
                else:
                    output.unlink()
            except OSError as rollback_error:
                rollback_errors.append(f"{output}: {rollback_error}")
        if rollback_errors:
            raise RuntimeError(f"publish failed: {error}; rollback failed: " +
                               "; ".join(rollback_errors)) from error
        raise


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--language", choices=(*LANGUAGES, "both"), default="both")
    parser.add_argument("--build-root", type=Path, default=ROOT / "build")
    parser.add_argument("--output-root", type=Path, default=ROOT / "releases")
    parser.add_argument("--dry-run", action="store_true", help="show builds without changing files")
    parser.add_argument("--no-clean", action="store_true", help="reuse script-owned build directories")
    return parser.parse_args()


def main():
    args = parse_args()
    if not (SDK / "tools/build_tools/build_files/project_main.mk").is_file():
        raise RuntimeError(f"BK7258 SDK is unavailable at {SDK}")
    lines, found, options = source_config()
    python = select_python() if not args.dry_run else None
    build_root = args.build_root.expanduser().resolve()
    output_root = args.output_root.expanduser().resolve()
    if ROOT.is_relative_to(build_root) or \
            (build_root.is_relative_to(ROOT) and
             not build_root.is_relative_to(ROOT / "build")):
        raise RuntimeError("build root must be outside the repository or under its build directory")
    if ROOT.is_relative_to(output_root) or \
            (output_root.is_relative_to(ROOT) and
             not output_root.is_relative_to(ROOT / "releases")):
        raise RuntimeError("output root must be outside the repository or under its releases directory")
    env = os.environ.copy()
    if python:
        env["PATH"] = f"{python.parent}{os.pathsep}{env.get('PATH', '')}"
    env.setdefault("CCACHE_DISABLE", "1")
    languages = LANGUAGES if args.language == "both" else (args.language,)
    outputs = []
    started = time.perf_counter()
    for language in languages:
        name = f"bk7258-{language}"
        variant = build_root / f"build-{name}"
        if output_root == variant or output_root.is_relative_to(variant):
            raise RuntimeError("output directory cannot be inside a variant build")
    if not args.dry_run:
        build_root.mkdir(parents=True, exist_ok=True)
        output_root.mkdir(parents=True, exist_ok=True)
    staging = (tempfile.TemporaryDirectory(prefix=".mybot-build-all-", dir=output_root)
               if not args.dry_run else nullcontext())
    with staging as staged:
        for language in languages:
            name = f"bk7258-{language}"
            variant = build_root / f"build-{name}"
            image_name = f"{name}.bin"
            command = ["make", "-C", str(variant / "solution/projects/mybot"),
                       "bk7258", f"SDK_DIR={SDK}"]
            print(f"\nBuilding {name}", flush=True)
            print(">>>", " ".join(command), flush=True)
            if not args.dry_run:
                variant_started = time.perf_counter()
                project = prepare_variant(
                    variant, variant_config(lines, found, options, language), not args.no_clean)
                subprocess.run(command, env=env, check=True)
                image = check_firmware(project, language)
                shutil.copyfile(image, Path(staged) / image_name)
                print(f"[OK] {name} ({elapsed(time.perf_counter() - variant_started)})", flush=True)
            outputs.append(output_root / image_name)
        if not args.dry_run:
            publish_outputs(staged, outputs)

    print(f"\nTotal elapsed: {elapsed(time.perf_counter() - started)}", flush=True)
    for output in outputs:
        print(output, flush=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        sys.exit(1)

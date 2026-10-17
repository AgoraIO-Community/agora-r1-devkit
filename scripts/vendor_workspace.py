#!/usr/bin/env python3
"""Prepare pinned, patched BK sources without modifying the official checkouts."""

import hashlib
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
LOCK = ROOT / "vendor.lock.json"
MARKER = "mybot-bk7258-vendor-workspace-v1"
REPOSITORIES = ("bk_avdk_smp", "bk_solution_ai")
COPY_IGNORES = shutil.ignore_patterns("build", "build_*", "cmake-build-*", "releases", "__pycache__", "*.pyc", ".git")


def git(directory, *arguments):
    result = subprocess.run(
        ["git", "-C", str(directory), *arguments], check=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    return result.stdout


def digest(data):
    return hashlib.sha256(data).hexdigest()


def load_lock(root=ROOT):
    """Validate the manifest and patch contents before touching any build tree."""
    root = Path(root).resolve()
    path = root / LOCK.name
    manifest = json.loads(path.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 1 or \
            set(manifest.get("repositories", {})) != set(REPOSITORIES):
        raise RuntimeError(f"unsupported vendor manifest: {path}")
    for name, repository in manifest["repositories"].items():
        commit = repository.get("commit", "")
        if len(commit) != 40 or any(character not in "0123456789abcdef" for character in commit):
            raise RuntimeError(f"invalid pinned commit for {name}")
        if repository.get("url") != f"https://github.com/bekencorp/{name}.git":
            raise RuntimeError(f"{name} must use its BK official repository")
        for patch in repository.get("patches", []):
            relative = Path(patch["path"])
            if relative.is_absolute() or ".." in relative.parts or \
                    not relative.is_relative_to(Path("patches") / name):
                raise RuntimeError(f"patch must be under patches/{name}: {relative}")
            patch_path = root / relative
            if patch_path.is_symlink() or not patch_path.is_file() or \
                    digest(patch_path.read_bytes()) != patch.get("sha256"):
                raise RuntimeError(f"patch checksum mismatch: {patch_path}")
    fingerprint = digest(json.dumps(manifest, sort_keys=True).encode("utf-8"))
    return manifest, fingerprint


def validate_sources(root, manifest):
    for name, repository in manifest["repositories"].items():
        configured_url = git(root, "config", "--file", str(root / ".gitmodules"),
                             "--get", f"submodule.{name}.url").decode().strip()
        if configured_url != repository["url"]:
            raise RuntimeError(f"{name} .gitmodules URL does not match the official vendor lock")
        source = root / name
        if source.is_symlink() or not source.is_dir():
            raise RuntimeError(f"official checkout is unavailable: {source}")
        if Path(git(source, "rev-parse", "--show-toplevel").decode().strip()).resolve() != source:
            raise RuntimeError(f"not a standalone official checkout: {source}")
        actual = git(source, "rev-parse", "HEAD").decode().strip()
        if actual != repository["commit"]:
            raise RuntimeError(f"{name} HEAD is {actual}; expected {repository['commit']}; "
                               "run git submodule update --init")
        if git(source, "status", "--porcelain", "--untracked-files=no").strip():
            raise RuntimeError(f"official checkout has tracked changes: {source}")


def validate_stage(stage, manifest, fingerprint):
    marker = stage / ".mybot-vendor-workspace.json"
    if stage.is_symlink() or not stage.is_dir() or marker.is_symlink() or not marker.is_file():
        raise RuntimeError(f"refusing an unowned vendor workspace: {stage}")
    saved = json.loads(marker.read_text(encoding="utf-8"))
    if not isinstance(saved, dict) or saved.get("owner") != MARKER or \
            saved.get("fingerprint") != fingerprint or not isinstance(saved.get("diffs"), dict):
        raise RuntimeError(f"vendor workspace ownership does not match: {stage}")
    for name, repository in manifest["repositories"].items():
        checkout = stage / name
        if checkout.is_symlink() or \
                git(checkout, "rev-parse", "HEAD").decode().strip() != repository["commit"] or \
                git(checkout, "diff", "--binary").strip() or \
                digest(git(checkout, "diff", "--cached", "--binary", "HEAD")) != \
                saved["diffs"].get(name):
            raise RuntimeError(f"prepared vendor sources were modified: {checkout}; "
                               "choose a new --build-root to rebuild safely")


def sync_product(source, destination):
    """Replace only the product overlay owned by this prepared workspace."""
    if not source.is_dir() or source.is_symlink():
        raise RuntimeError(f"MyBot component sources are unavailable: {source}")
    if destination.is_symlink() or (destination.exists() and not destination.is_dir()):
        raise RuntimeError(f"unexpected product overlay: {destination}")
    temporary = Path(tempfile.mkdtemp(prefix=".mybot-overlay-", dir=destination.parent))
    try:
        shutil.copytree(source, temporary, dirs_exist_ok=True, ignore=COPY_IGNORES)
        if destination.exists():
            shutil.rmtree(destination)
        os.replace(temporary, destination)
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


def prepare_vendor(build_root, root=ROOT, dry_run=False):
    root = Path(root).resolve()
    manifest, fingerprint = load_lock(root)
    validate_sources(root, manifest)
    vendor_root = Path(build_root) / "_vendor"
    stage = vendor_root / fingerprint
    if dry_run:
        return stage
    if vendor_root.is_symlink() or (vendor_root.exists() and not vendor_root.is_dir()):
        raise RuntimeError(f"unexpected vendor build directory: {vendor_root}")
    vendor_root.mkdir(parents=True, exist_ok=True)
    if not stage.exists() and not stage.is_symlink():
        temporary = Path(tempfile.mkdtemp(prefix=".prepare-", dir=vendor_root))
        try:
            diffs = {}
            for name, repository in manifest["repositories"].items():
                checkout = temporary / name
                subprocess.run(
                    ["git", "clone", "--quiet", "--local", "--no-hardlinks", "--dissociate", "--no-checkout",
                     str(root / name), str(checkout)], check=True,
                )
                git(checkout, "checkout", "--quiet", "--detach", repository["commit"])
                if name == "bk_solution_ai" and git(checkout, "ls-files", "components/mybot"):
                    raise RuntimeError("the pinned Solution already owns components/mybot")
                for patch in repository.get("patches", []):
                    patch_path = root / patch["path"]
                    git(checkout, "apply", "--check", str(patch_path))
                    git(checkout, "apply", "--index", str(patch_path))
                diffs[name] = digest(git(checkout, "diff", "--cached", "--binary", "HEAD"))
            marker = {"owner": MARKER, "fingerprint": fingerprint, "diffs": diffs}
            (temporary / ".mybot-vendor-workspace.json").write_text(
                json.dumps(marker, indent=2) + "\n", encoding="utf-8")
            os.replace(temporary, stage)
        finally:
            if temporary.exists():
                shutil.rmtree(temporary)
    validate_stage(stage, manifest, fingerprint)
    sync_product(root / "components/mybot", stage / "bk_solution_ai/components/mybot")
    return stage

import json
import subprocess
import tempfile
import unittest
from pathlib import Path

import vendor_workspace as vendor


class VendorWorkspaceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "source"
        self.root.mkdir()
        self.build = Path(self.temporary.name) / "build"
        self.manifest = {"schema_version": 1, "repositories": {}}
        for name in vendor.REPOSITORIES:
            repository = self.root / name
            repository.mkdir()
            vendor.git(repository, "init", "--quiet")
            vendor.git(repository, "config", "user.email", "test@example.invalid")
            vendor.git(repository, "config", "user.name", "Vendor Test")
            (repository / "components").mkdir()
            (repository / "components/vendor.txt").write_text("original\n", encoding="utf-8")
            vendor.git(repository, "add", ".")
            vendor.git(repository, "commit", "--quiet", "-m", "baseline")
            commit = vendor.git(repository, "rev-parse", "HEAD").decode().strip()
            self.manifest["repositories"][name] = {
                "url": f"https://github.com/bekencorp/{name}.git",
                "commit": commit,
                "patches": [],
            }
        self.product = self.root / "components/mybot"
        self.product.mkdir(parents=True)
        (self.product / "product.c").write_text("version one\n", encoding="utf-8")
        self.set_patch("bk_avdk_smp", "patched")
        self.save_lock()

    def save_lock(self):
        (self.root / "vendor.lock.json").write_text(json.dumps(self.manifest), encoding="utf-8")
        (self.root / ".gitmodules").write_text("".join(
            f'[submodule "{name}"]\n\tpath = {name}\n\turl = {repository["url"]}\n'
            for name, repository in self.manifest["repositories"].items()), encoding="utf-8")

    def set_patch(self, name, replacement, original="original"):
        relative = Path("patches") / name / "0001-test.patch"
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            "diff --git a/components/vendor.txt b/components/vendor.txt\n"
            "--- a/components/vendor.txt\n+++ b/components/vendor.txt\n"
            f"@@ -1 +1 @@\n-{original}\n+{replacement}\n", encoding="utf-8")
        self.manifest["repositories"][name]["patches"] = [
            {"path": str(relative), "sha256": vendor.digest(path.read_bytes())}]

    def prepare(self, **kwargs):
        return vendor.prepare_vendor(self.build, root=self.root, **kwargs)

    def test_applies_only_to_isolated_repos_and_copies_current_product(self):
        stage = self.prepare()
        self.assertEqual((stage / "bk_avdk_smp/components/vendor.txt").read_text(), "patched\n")
        self.assertEqual((stage / "bk_solution_ai/components/mybot/product.c").read_text(),
                         "version one\n")
        self.assertFalse((stage / "bk_solution_ai/components/mybot").is_symlink())
        for name in vendor.REPOSITORIES:
            self.assertEqual((self.root / name / "components/vendor.txt").read_text(), "original\n")
            self.assertFalse(vendor.git(self.root / name, "status", "--porcelain").strip())
            self.assertEqual(Path(vendor.git(stage / name, "rev-parse", "--show-toplevel")
                                  .decode().strip()), stage / name)

    def test_rejects_wrong_baseline_before_creating_build_tree(self):
        self.manifest["repositories"]["bk_avdk_smp"]["commit"] = "a" * 40
        self.save_lock()
        with self.assertRaisesRegex(RuntimeError, "expected"):
            self.prepare()
        self.assertFalse(self.build.exists())

    def test_rejects_personal_gitmodule_url_before_creating_build_tree(self):
        path = self.root / ".gitmodules"
        path.write_text(path.read_text().replace("bekencorp/bk_avdk_smp", "someone/bk_avdk_smp"))
        with self.assertRaisesRegex(RuntimeError, "official vendor lock"):
            self.prepare()
        self.assertFalse(self.build.exists())

    def test_prepared_git_objects_do_not_borrow_from_source(self):
        stage = self.prepare()
        for name in vendor.REPOSITORIES:
            checkout = stage / name
            self.assertFalse((checkout / ".git/objects/info/alternates").exists())
            original = self.root / name
            moved = self.root / f"{name}-moved"
            original.rename(moved)
            self.assertEqual(vendor.git(checkout, "show", "HEAD:components/vendor.txt"), b"original\n")
            moved.rename(original)

    def test_product_build_outputs_are_not_copied(self):
        cache = self.product / "mybot_sdk/build/generated.o"
        cache.parent.mkdir(parents=True)
        cache.write_bytes(b"build object")
        stage = self.prepare()
        self.assertFalse((stage / "bk_solution_ai/components/mybot/mybot_sdk/build").exists())

    def test_rejects_dirty_official_checkout(self):
        path = self.root / "bk_solution_ai/components/vendor.txt"
        path.write_text("user change\n", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "tracked changes"):
            self.prepare()
        self.assertEqual(path.read_text(), "user change\n")

    def test_rejects_tampered_patch_without_creating_build_tree(self):
        path = self.root / self.manifest["repositories"]["bk_avdk_smp"]["patches"][0]["path"]
        path.write_text("modified patch\n", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "checksum"):
            self.prepare()
        self.assertFalse(self.build.exists())

    def test_failed_patch_has_no_reusable_partial_workspace(self):
        self.set_patch("bk_avdk_smp", "patched", original="does not match")
        self.save_lock()
        with self.assertRaises(subprocess.CalledProcessError):
            self.prepare()
        self.assertEqual(list((self.build / "_vendor").iterdir()), [])
        self.assertFalse(vendor.git(self.root / "bk_avdk_smp", "status", "--porcelain").strip())

    def test_patch_lock_change_selects_new_workspace(self):
        first = self.prepare()
        self.set_patch("bk_avdk_smp", "second patch")
        self.save_lock()
        second = self.prepare()
        self.assertNotEqual(first, second)
        self.assertEqual((first / "bk_avdk_smp/components/vendor.txt").read_text(), "patched\n")
        self.assertEqual((second / "bk_avdk_smp/components/vendor.txt").read_text(), "second patch\n")

    def test_product_updates_and_removals_are_refreshed_in_reused_workspace(self):
        first = self.prepare()
        (self.product / "product.c").unlink()
        (self.product / "updated.c").write_text("version two\n", encoding="utf-8")
        second = self.prepare()
        self.assertEqual(first, second)
        overlay = second / "bk_solution_ai/components/mybot"
        self.assertFalse((overlay / "product.c").exists())
        self.assertEqual((overlay / "updated.c").read_text(), "version two\n")

    def test_refuses_user_changes_in_prepared_vendor(self):
        stage = self.prepare()
        changed = stage / "bk_avdk_smp/components/vendor.txt"
        changed.write_text("manual edit\n", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "modified"):
            self.prepare()
        self.assertEqual(changed.read_text(), "manual edit\n")

    def test_added_patch_files_are_included_in_tamper_detection(self):
        name = "bk_solution_ai"
        relative = Path("patches") / name / "0002-added-file.patch"
        path = self.root / relative
        path.parent.mkdir(parents=True)
        path.write_text(
            "diff --git a/components/added.c b/components/added.c\n"
            "new file mode 100644\n--- /dev/null\n+++ b/components/added.c\n"
            "@@ -0,0 +1 @@\n+added by patch\n", encoding="utf-8")
        self.manifest["repositories"][name]["patches"] = [
            {"path": str(relative), "sha256": vendor.digest(path.read_bytes())}]
        self.save_lock()
        stage = self.prepare()
        added = stage / name / "components/added.c"
        self.assertEqual(added.read_text(), "added by patch\n")
        self.prepare()
        added.write_text("manual change\n", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "modified"):
            self.prepare()

    def test_binary_archive_patch_roundtrip_and_tamper_detection(self):
        name = "bk_avdk_smp"
        repository = self.root / name
        archive = repository / "components/libaec.a"
        original = b"!<arch>\n" + bytes(range(256)) * 4
        replacement = b"!<arch>\n" + bytes(reversed(range(256))) * 5
        archive.write_bytes(original)
        vendor.git(repository, "add", "components/libaec.a")
        vendor.git(repository, "commit", "--quiet", "-m", "binary baseline")
        self.manifest["repositories"][name]["commit"] = \
            vendor.git(repository, "rev-parse", "HEAD").decode().strip()
        archive.write_bytes(replacement)
        payload = vendor.git(repository, "diff", "--binary", "HEAD")
        self.assertIn(b"GIT binary patch\n", payload)
        archive.write_bytes(original)
        path = self.root / "patches/bk_avdk_smp/0002-aec-archive.patch"
        path.write_bytes(payload)
        self.manifest["repositories"][name]["patches"].append(
            {"path": str(path.relative_to(self.root)), "sha256": vendor.digest(payload)})
        self.save_lock()

        stage = self.prepare()
        patched = stage / name / "components/libaec.a"
        self.assertEqual(patched.read_bytes(), replacement)
        self.assertEqual(archive.read_bytes(), original)
        self.assertFalse(vendor.git(repository, "status", "--porcelain").strip())
        self.assertEqual(self.prepare(), stage)
        patched.write_bytes(replacement + b"tampered")
        with self.assertRaisesRegex(RuntimeError, "modified"):
            self.prepare()

    def test_dry_run_validates_without_creating_files(self):
        self.assertEqual(self.prepare(dry_run=True).parent, self.build / "_vendor")
        self.assertFalse(self.build.exists())

    def test_refuses_unowned_workspace_without_removing_it(self):
        stage = self.prepare(dry_run=True)
        stage.mkdir(parents=True)
        (stage / "precious.txt").write_text("keep", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "unowned"):
            self.prepare()
        self.assertEqual((stage / "precious.txt").read_text(), "keep")


if __name__ == "__main__":
    unittest.main()

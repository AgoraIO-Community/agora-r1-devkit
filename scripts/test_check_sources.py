import hashlib
import json
import tempfile
import unittest
from pathlib import Path

import check_sources


class SnapshotTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.product = self.root / "components/mybot"
        self.files = {}
        for directory in check_sources.MANAGED:
            path = self.product / directory / "original.h"
            path.parent.mkdir(parents=True)
            path.write_bytes(b"preserved source")
            self.files[path.relative_to(self.product).as_posix()] = \
                hashlib.sha256(path.read_bytes()).hexdigest()
        self.save()

    def save(self):
        (self.product / "snapshots.json").write_text(json.dumps({
            "schema_version": 1, "files": self.files}), encoding="utf-8")

    def test_accepts_product_wrappers_and_ignores_build_outputs(self):
        (self.product / "mybot_sdk/CMakeLists.txt").write_text("editable wrapper")
        cache = self.product / "mybot_aosl/aosl/build/generated.o"
        cache.parent.mkdir()
        cache.write_bytes(b"generated")
        self.assertEqual(check_sources.check_snapshots(self.root), len(self.files))

    def test_rejects_modified_snapshot(self):
        (self.product / "mybot_sdk/src/original.h").write_text("changed source")
        with self.assertRaisesRegex(RuntimeError, "checksum mismatch"):
            check_sources.check_snapshots(self.root)

    def test_rejects_unrecorded_source(self):
        (self.product / "mybot_sdk/platforms/extra.c").write_text("extra source")
        with self.assertRaisesRegex(RuntimeError, "unrecorded"):
            check_sources.check_snapshots(self.root)

    def test_rejects_symlinked_source(self):
        path = self.product / "mybot_sdk/src/original.h"
        path.unlink()
        path.symlink_to(self.product / "mybot_sdk/include/original.h")
        with self.assertRaisesRegex(RuntimeError, "linked product snapshot"):
            check_sources.check_snapshots(self.root)


if __name__ == "__main__":
    unittest.main()

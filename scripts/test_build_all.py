import ast
import base64
import hashlib
import io
import os
import re
import subprocess
import tempfile
import unittest
from argparse import Namespace
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch

import build_all
import prepare_vendor


class BuildAllTests(unittest.TestCase):
    def regional_pems(self):
        header = (build_all.COMPONENTS / "mybot/mybot_sdk/platforms/bk725x/modules/network/"
                  "mybot_tls_roots.h").read_text(encoding="utf-8")
        bodies = re.findall(r"MYBOT_TLS_ROOTS_PEM\[\]\s*=\s*(.*?);", header, re.DOTALL)
        pems = ["".join(ast.literal_eval(value) for value in
                        re.findall(r'"(?:[^"\\]|\\.)*"', body)).encode("ascii")
                for body in bodies]
        self.assertEqual(len(pems), 2)
        return dict(zip(build_all.LANGUAGES, pems))

    def test_regional_tls_roots_match_latest_single_anchor_policy(self):
        pems = self.regional_pems()
        for language, pem in pems.items():
            encoded = b"".join(pem.splitlines()[1:-1])
            fingerprint = hashlib.sha256(base64.b64decode(encoded)).hexdigest()
            self.assertEqual(fingerprint, build_all.TRUST_ANCHORS[language])

    def test_regional_endpoints_match_sdk_addresses_and_platform_config(self):
        expected = {
            "zh-CN": b"https://botstation.shengwang.cn/api",
            "en-US": b"https://botstation.agora.io/api",
        }
        self.assertEqual(build_all.ENDPOINTS, expected)
        header = (build_all.COMPONENTS / "mybot/mybot_sdk/platforms/bk725x/modules/controller/"
                  "mybot_language.h").read_text(encoding="utf-8")
        branches = re.findall(
            r"(?:#ifdef|#elif defined\()\s*(CONFIG_MYBOT_LANGUAGE_\w+)\)?\s*"
            r'\n#define MYBOT_SERVER_BASE "([^"]+)"', header)
        self.assertEqual(dict(branches), {
            "CONFIG_MYBOT_LANGUAGE_ZH_CN": expected["zh-CN"].decode("ascii"),
            "CONFIG_MYBOT_LANGUAGE_EN_US": expected["en-US"].decode("ascii"),
        })

    def fixture(self, temporary, english=False):
        root = Path(temporary)
        project = root / "projects/mybot"
        config = project / "ap/config/bk7258_ap/config"
        config.parent.mkdir(parents=True)
        original = ("# CONFIG_MYBOT_LANGUAGE_ZH_CN is not set\nCONFIG_MYBOT_LANGUAGE_EN_US=y\n"
                    if english else
                    "CONFIG_MYBOT_LANGUAGE_ZH_CN=y\n# CONFIG_MYBOT_LANGUAGE_EN_US is not set\n")
        config.write_text(original + "CONFIG_OTHER_FEATURE=y\n", encoding="utf-8")
        (project / "cp/config/bk7258").mkdir(parents=True)
        (project / "cp/config/bk7258/config").write_text("CONFIG_CP=y\n")
        return root, project, config

    def test_language_variants_preserve_english_source_and_other_settings(self):
        with tempfile.TemporaryDirectory() as temporary:
            _, project, config = self.fixture(temporary, english=True)
            original = config.read_bytes()
            with patch.object(build_all, "PROJECT", project):
                lines, found, options = build_all.source_config()
            for language in build_all.LANGUAGES:
                selected = build_all.variant_config(lines, found, options, language)
                symbol = options[language]
                other = options["en-US" if language == "zh-CN" else "zh-CN"]
                self.assertIn(f"{symbol}=y\n", selected)
                self.assertIn(f"# {other} is not set\n", selected)
                self.assertIn("CONFIG_OTHER_FEATURE=y\n", selected)
            self.assertEqual(config.read_bytes(), original)

    def test_rejects_ambiguous_language_config(self):
        with tempfile.TemporaryDirectory() as temporary:
            _, project, config = self.fixture(temporary)
            config.write_text("CONFIG_MYBOT_LANGUAGE_ZH_CN=y\nCONFIG_MYBOT_LANGUAGE_EN_US=y\n")
            with patch.object(build_all, "PROJECT", project):
                with self.assertRaisesRegex(RuntimeError, "exactly one active"):
                    build_all.source_config()

    def test_prepare_cli_uses_config_copies_and_prepared_sdk(self):
        with tempfile.TemporaryDirectory() as temporary:
            root, project, config = self.fixture(temporary, english=True)
            original = config.read_bytes()
            stage = root / "prepared"
            components = stage / "bk_solution_ai/components"
            components.mkdir(parents=True)
            build_root = root / "build"
            with patch.object(build_all, "ROOT", root), \
                    patch.object(build_all, "PROJECT", project), \
                    patch.object(build_all, "check_product_sources"), \
                    patch.object(prepare_vendor, "prepare_vendor", return_value=stage), \
                    patch("sys.argv", ["prepare_vendor.py", "--language", "zh-CN",
                                       "--build-root", str(build_root)]), \
                    redirect_stdout(io.StringIO()) as output:
                self.assertEqual(prepare_vendor.main(), 0)
            copied = build_root / "build-bk7258-zh-CN/solution/projects/mybot"
            self.assertIn("CONFIG_MYBOT_LANGUAGE_ZH_CN=y\n",
                          (copied / "ap/config/bk7258_ap/config").read_text())
            self.assertEqual((copied / "cp/config/bk7258/config").read_text(), "CONFIG_CP=y\n")
            self.assertEqual(config.read_bytes(), original)
            self.assertIn(f"SDK_DIR={stage / 'bk_avdk_smp'}", output.getvalue())

    def test_no_clean_refreshes_sources_and_preserves_cache_for_same_sdk(self):
        with tempfile.TemporaryDirectory() as temporary:
            root, project, _ = self.fixture(temporary)
            components = root / "components"
            components.mkdir()
            variant = root / "variant"
            with patch.object(build_all, "PROJECT", project):
                copy = build_all.prepare_variant(variant, "first\n", True, components)
                (copy / "stale.h").write_text("stale")
                cache = copy / "build/cache"
                cache.parent.mkdir()
                cache.write_text("cached")
                (project / "new.h").write_text("current")
                build_all.prepare_variant(variant, "second\n", False, components)
            self.assertFalse((copy / "stale.h").exists())
            self.assertEqual((copy / "new.h").read_text(), "current")
            self.assertEqual(cache.read_text(), "cached")
            self.assertEqual((copy / "ap/config/bk7258_ap/config").read_text(), "second\n")

    def test_changed_vendor_invalidates_no_clean_cache(self):
        with tempfile.TemporaryDirectory() as temporary:
            root, project, _ = self.fixture(temporary)
            first = root / "vendor-first/components"
            second = root / "vendor-second/components"
            first.mkdir(parents=True)
            second.mkdir(parents=True)
            with patch.object(build_all, "PROJECT", project):
                copy = build_all.prepare_variant(root / "variant", "first\n", True, first)
                (copy / "build").mkdir()
                (copy / "build/cache").write_text("old objects")
                build_all.prepare_variant(root / "variant", "second\n", False, second)
            self.assertFalse((copy / "build").exists())
            self.assertEqual((root / "variant/solution/components").resolve(), second)

    def test_firmware_requires_cp_and_selected_language_endpoint(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary)
            built = project / "build/bk7258/mybot"
            image = built / "package/all-app.bin"
            config = built / "bk7258_ap/config/sdkconfig.h"
            elf = built / "bk7258_ap/app.elf"
            cp = built / "bk7258/app.bin"
            image.parent.mkdir(parents=True)
            config.parent.mkdir(parents=True)
            cp.parent.mkdir()
            image.write_bytes(b"combined")
            cp.write_bytes(b"cp")
            pems = self.regional_pems()
            for language, symbol in (("zh-CN", "ZH_CN"), ("en-US", "EN_US")):
                config.write_text(f"#define CONFIG_MYBOT_LANGUAGE_{symbol} 1\n")
                elf.write_bytes(build_all.endpoint(language) + b"\0" + pems[language])
                self.assertEqual(build_all.check_firmware(project, language), image)
                other = "en-US" if language == "zh-CN" else "zh-CN"
                with self.assertRaisesRegex(RuntimeError, "configuration does not match"):
                    build_all.check_firmware(project, other)
                for wrong_roots in (b"", pems[other], pems[language] + pems[other]):
                    elf.write_bytes(build_all.endpoint(language) + b"\0" + wrong_roots)
                    with self.assertRaisesRegex(RuntimeError, "regional TLS trust anchor"):
                        build_all.check_firmware(project, language)
            elf.write_bytes(build_all.endpoint("en-US") + b"\0" + build_all.endpoint("zh-CN"))
            with self.assertRaisesRegex(RuntimeError, "endpoint"):
                build_all.check_firmware(project, "en-US")
            elf.write_bytes(build_all.endpoint("en-US") + b"\0" + pems["en-US"])
            cp.unlink()
            with self.assertRaisesRegex(RuntimeError, "missing AP/CP"):
                build_all.check_firmware(project, "en-US")

    def test_publish_restores_both_previous_images_after_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            staged = root / "staged"
            staged.mkdir()
            outputs = [root / "zh.bin", root / "en.bin"]
            for output in outputs:
                output.write_bytes(b"previous")
                (staged / output.name).write_bytes(b"current")
            real_replace = os.replace

            def fail_second(source, target):
                if Path(source) == staged / "en.bin":
                    raise OSError("simulated publish failure")
                return real_replace(source, target)

            with patch.object(build_all.os, "replace", side_effect=fail_second):
                with self.assertRaisesRegex(OSError, "simulated"):
                    build_all.publish_outputs(staged, outputs)
            for output in outputs:
                self.assertEqual(output.read_bytes(), b"previous")

    def test_publish_rejects_non_file_target_before_replacing_any_image(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            staged = root / "staged"
            staged.mkdir()
            first = root / "zh.bin"
            second = root / "en.bin"
            first.write_bytes(b"previous")
            second.mkdir()
            (staged / first.name).write_bytes(b"current")
            (staged / second.name).write_bytes(b"current")
            with self.assertRaisesRegex(RuntimeError, "not a regular file"):
                build_all.publish_outputs(staged, [first, second])
            self.assertEqual(first.read_bytes(), b"previous")

    def test_second_build_failure_preserves_previous_firmware_pair(self):
        with tempfile.TemporaryDirectory() as temporary:
            root, project, _ = self.fixture(temporary)
            vendor = root / "prepared"
            (vendor / "bk_solution_ai/components").mkdir(parents=True)
            outputs = root / "releases"
            outputs.mkdir()
            images = [outputs / f"bk7258-{language}.bin" for language in build_all.LANGUAGES]
            for image in images:
                image.write_bytes(b"previous")
            built = root / "new-image.bin"
            built.write_bytes(b"new")
            args = Namespace(language="both", build_root=root / "build", output_root=outputs,
                             dry_run=False, no_clean=False)
            with patch.object(build_all, "ROOT", root), \
                    patch.object(build_all, "PROJECT", project), \
                    patch.object(build_all, "parse_args", return_value=args), \
                    patch.object(build_all, "check_product_sources"), \
                    patch.object(build_all, "select_python", return_value=Path("/usr/bin/python3")), \
                    patch.object(build_all, "prepare_vendor", return_value=vendor), \
                    patch.object(build_all, "check_firmware", return_value=built), \
                    patch.object(build_all.subprocess, "run", side_effect=[
                        None, subprocess.CalledProcessError(1, "make")]) as run, \
                    redirect_stdout(io.StringIO()):
                with self.assertRaises(subprocess.CalledProcessError):
                    build_all.main()
            for call in run.call_args_list:
                self.assertIn(f"SDK_DIR={vendor / 'bk_avdk_smp'}", call.args[0])
            for image in images:
                self.assertEqual(image.read_bytes(), b"previous")


if __name__ == "__main__":
    unittest.main()

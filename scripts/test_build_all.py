import os
import re
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import build_all


class BuildAllTests(unittest.TestCase):
    def test_regional_endpoints_match_sdk_addresses_and_platform_config(self):
        expected = {
            "zh-CN": b"https://botstation.shengwang.cn/api",
            "en-US": b"https://botstation.agora.io/api",
        }
        self.assertEqual(build_all.ENDPOINTS, expected)
        header = (build_all.COMPONENTS / "mybot/platforms/bk725x/modules/controller/"
                  "mybot_language.h").read_text(encoding="utf-8")
        branches = re.findall(
            r"(?:#ifdef|#elif defined\()\s*(CONFIG_MYBOT_LANGUAGE_\w+)\)?\s*"
            r"\n#define MYBOT_SERVER_BASE \"([^\"]+)\"", header)
        self.assertEqual(dict(branches), {
            "CONFIG_MYBOT_LANGUAGE_ZH_CN": expected["zh-CN"].decode("ascii"),
            "CONFIG_MYBOT_LANGUAGE_EN_US": expected["en-US"].decode("ascii"),
        })

    def test_bk7258_language_selection_and_firmware_checks(self):
        lines, found, options = build_all.source_config()
        english = build_all.variant_config(lines, found, options, "en-US")
        self.assertIn("CONFIG_MYBOT_LANGUAGE_EN_US=y\n", english)
        self.assertIn("# CONFIG_MYBOT_LANGUAGE_ZH_CN is not set\n", english)
        self.assertEqual("".join(lines), build_all.variant_config(
            lines, found, options, "zh-CN"))

        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary)
            built = project / "build/bk7258/mybot"
            ap = built / "bk7258_ap"
            (built / "package").mkdir(parents=True)
            (ap / "config").mkdir(parents=True)
            (built / "package/all-app.bin").write_bytes(b"firmware")
            config = ap / "config/sdkconfig.h"
            elf = ap / "app.elf"

            for language, symbol in (("zh-CN", "ZH_CN"), ("en-US", "EN_US")):
                config.write_text(f"#define CONFIG_MYBOT_LANGUAGE_{symbol} 1\n",
                                  encoding="utf-8")
                elf.write_bytes(build_all.ENDPOINTS[language])
                self.assertEqual(build_all.check_firmware(project, language),
                                 built / "package/all-app.bin")
                with self.assertRaisesRegex(RuntimeError, "configuration does not match"):
                    build_all.check_firmware(project,
                                             "en-US" if language == "zh-CN" else "zh-CN")

            elf.write_bytes(build_all.ENDPOINTS["zh-CN"] + build_all.ENDPOINTS["en-US"])
            with self.assertRaisesRegex(RuntimeError, "wrong device-service endpoint"):
                build_all.check_firmware(project, "en-US")

    def test_no_clean_removes_stale_source_but_keeps_build_cache(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            project = root / "source"
            config = project / "ap/config/bk7258_ap/config"
            config.parent.mkdir(parents=True)
            config.write_text("original\n", encoding="utf-8")
            components = root / "components"
            components.mkdir()
            variant = root / "variant"

            with patch.object(build_all, "PROJECT", project), \
                    patch.object(build_all, "COMPONENTS", components):
                copy = build_all.prepare_variant(variant, "first\n", clean=True)
                (copy / "stale.h").write_text("stale", encoding="utf-8")
                cache = copy / "build/cache"
                cache.parent.mkdir()
                cache.write_text("cached", encoding="utf-8")
                (project / "new.h").write_text("current", encoding="utf-8")

                build_all.prepare_variant(variant, "second\n", clean=False)

            self.assertFalse((copy / "stale.h").exists())
            self.assertEqual((copy / "new.h").read_text(encoding="utf-8"), "current")
            self.assertEqual(cache.read_text(encoding="utf-8"), "cached")
            self.assertEqual((copy / "ap/config/bk7258_ap/config").read_text(
                encoding="utf-8"), "second\n")

    def test_publish_restores_previous_pair_on_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            staged = root / "staged"
            staged.mkdir()
            outputs = [root / "zh.bin", root / "en.bin"]
            for output in outputs:
                output.write_bytes(b"previous")
                (staged / output.name).write_bytes(b"current")

            real_replace = os.replace

            def fail_second_image(source, target):
                if Path(source) == staged / "en.bin":
                    raise OSError("simulated publish failure")
                return real_replace(source, target)

            with patch.object(build_all.os, "replace", side_effect=fail_second_image):
                with self.assertRaisesRegex(OSError, "simulated publish failure"):
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


if __name__ == "__main__":
    unittest.main()

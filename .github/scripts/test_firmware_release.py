#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


SCRIPT_PATH = Path(__file__).with_name("firmware_release.py")
SPEC = importlib.util.spec_from_file_location("firmware_release", SCRIPT_PATH)
assert SPEC is not None
firmware_release = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = firmware_release
SPEC.loader.exec_module(firmware_release)


class ReleaseTagTest(unittest.TestCase):
    def test_next_release_tag_uses_first_same_day_suffix(self) -> None:
        def fake_run(command, check, capture_output, text):
            if "tag" in command:
                return subprocess.CompletedProcess(command, 0, stdout="fw-2026.06.10\n")
            return subprocess.CompletedProcess(command, 0, stdout="")

        with patch.object(firmware_release.subprocess, "run", side_effect=fake_run):
            self.assertEqual(
                firmware_release.next_release_tag("2026.06.10", Path("/tmp/repo")),
                "fw-2026.06.10.1",
            )

    def test_next_release_tag_uses_next_remote_same_day_suffix(self) -> None:
        remote_tags = "\n".join([
            "abc123\trefs/tags/fw-2026.06.10",
            "def456\trefs/tags/fw-2026.06.10.1",
            "fedcba\trefs/tags/fw-2026.06.10.1^{}",
        ])

        def fake_run(command, check, capture_output, text):
            if "ls-remote" in command:
                return subprocess.CompletedProcess(command, 0, stdout=remote_tags)
            return subprocess.CompletedProcess(command, 0, stdout="")

        with patch.object(firmware_release.subprocess, "run", side_effect=fake_run):
            self.assertEqual(
                firmware_release.next_release_tag("2026.06.10", Path("/tmp/repo")),
                "fw-2026.06.10.2",
            )

    def test_release_title_matches_suffixed_tag(self) -> None:
        self.assertEqual(
            firmware_release.release_title_from_tag("fw-2026.06.10.1"),
            "Firmware 2026.06.10.1",
        )


class TrmnlTargetTest(unittest.TestCase):
    def test_trmnl_source_change_builds_supported_platformio_targets(self) -> None:
        plan = firmware_release.build_plan(["examples/official/TRMNL/src/main.cpp"])
        target_ids = {target.id for target in plan.changed_targets}

        self.assertIn("TRMNL_reTerminal_E1001", target_ids)
        self.assertIn("TRMNL_reTerminal_E1002", target_ids)
        self.assertIn("TRMNL_reTerminal_E1003", target_ids)
        self.assertNotIn("TRMNL_reTerminal_E1004", target_ids)
        self.assertTrue(all(target.tool == "platformio" for target in plan.changed_targets))

    def test_trmnl_targets_use_fixed_version_and_expected_devices(self) -> None:
        targets = {
            target.id: target
            for target in firmware_release.FIRMWARE_TARGETS
            if target.id.startswith("TRMNL_")
        }

        self.assertEqual(targets["TRMNL_reTerminal_E1001"].devices, ("E1001",))
        self.assertEqual(targets["TRMNL_reTerminal_E1002"].devices, ("E1002",))
        self.assertEqual(targets["TRMNL_reTerminal_E1003"].devices, ("E1003",))
        self.assertTrue(all(target.fixed_version == "1.8.7" for target in targets.values()))
        self.assertEqual(targets["TRMNL_reTerminal_E1003"].app_offset, 0x20000)

    def test_semver_firmware_version_is_listed(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            firmware_dir = Path(temp_dir) / "TRMNL_reTerminal_E1001" / "1.8.7"
            firmware_dir.mkdir(parents=True)
            (firmware_dir / "manifest.json").write_text("{}\n", encoding="utf-8")

            self.assertEqual(
                firmware_release.date_versions(Path(temp_dir) / "TRMNL_reTerminal_E1001"),
                ["1.8.7"],
            )

    def test_manifest_can_use_trmnl_e1003_offsets(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            firmware_release.write_manifest(
                "TRMNL_reTerminal_E1003",
                "1.8.7",
                Path(temp_dir),
                boot_app0_offset=0x13000,
                app_offset=0x20000,
            )

            manifest = json.loads((Path(temp_dir) / "manifest.json").read_text(encoding="utf-8"))
            parts = manifest["builds"][0]["parts"]
            offsets = {part["path"]: part["offset"] for part in parts}

            self.assertEqual(offsets["boot_app0.bin"], 0x13000)
            self.assertEqual(offsets["TRMNL_reTerminal_E1003.ino.bin"], 0x20000)


if __name__ == "__main__":
    unittest.main()

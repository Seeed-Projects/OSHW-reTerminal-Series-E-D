#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import subprocess
import sys
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


if __name__ == "__main__":
    unittest.main()

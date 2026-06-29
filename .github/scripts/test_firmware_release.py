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
REPO_ROOT = SCRIPT_PATH.parents[2]
SPEC = importlib.util.spec_from_file_location("firmware_release", SCRIPT_PATH)
assert SPEC is not None
firmware_release = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = firmware_release
SPEC.loader.exec_module(firmware_release)


def platformio_section_flags(env_name: str) -> list[str]:
    platformio_ini = REPO_ROOT / "examples" / "official" / "TRMNL" / "platformio.ini"
    lines = platformio_ini.read_text(encoding="utf-8").splitlines()
    in_section = False
    flags: list[str] = []

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            in_section = stripped == f"[env:{env_name}]"
            continue
        if not in_section:
            continue
        if stripped.startswith("-D"):
            flags.append(stripped)

    return flags


def create_manifest_artifacts(firmware_dir: Path, firmware_id: str) -> None:
    (firmware_dir / f"{firmware_id}.ino.bootloader.bin").write_bytes(b"bootloader")
    (firmware_dir / f"{firmware_id}.ino.partitions.bin").write_bytes(b"partitions")
    (firmware_dir / f"{firmware_id}.ino.bin").write_bytes(b"app")
    (firmware_dir / "boot_app0.bin").write_bytes(b"boot_app0")


def path_without_query(path: str) -> str:
    return path.split("?", 1)[0]


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

    def test_trmnl_packaging_change_rebuilds_supported_platformio_targets(self) -> None:
        plan = firmware_release.build_plan([".github/scripts/firmware_release.py"])
        target_ids = {target.id for target in plan.changed_targets}

        self.assertIn("TRMNL_reTerminal_E1001", target_ids)
        self.assertIn("TRMNL_reTerminal_E1002", target_ids)
        self.assertIn("TRMNL_reTerminal_E1003", target_ids)
        self.assertNotIn("TRMNL_reTerminal_E1004", target_ids)

    def test_trmnl_workflow_change_rebuilds_supported_platformio_targets(self) -> None:
        plan = firmware_release.build_plan([".github/workflows/build-and-deploy.yml"])
        target_ids = {target.id for target in plan.changed_targets}

        self.assertIn("TRMNL_reTerminal_E1001", target_ids)
        self.assertIn("TRMNL_reTerminal_E1002", target_ids)
        self.assertIn("TRMNL_reTerminal_E1003", target_ids)
        self.assertNotIn("TRMNL_reTerminal_E1004", target_ids)

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
        self.assertTrue(targets["TRMNL_reTerminal_E1003"].include_filesystem)
        self.assertEqual(targets["TRMNL_reTerminal_E1003"].flash_size, "keep")

    def test_trmnl_platformio_targets_enable_serial_logging(self) -> None:
        targets = [
            target
            for target in firmware_release.FIRMWARE_TARGETS
            if target.id.startswith("TRMNL_reTerminal_E100")
            and target.pio_env
            and "E1004" not in target.devices
        ]

        for target in targets:
            with self.subTest(target=target.id):
                flags = platformio_section_flags(target.pio_env)
                self.assertIn("-D DEV_FIRMWARE=1", flags)

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
            firmware_dir = Path(temp_dir)
            create_manifest_artifacts(firmware_dir, "TRMNL_reTerminal_E1003")
            (firmware_dir / "TRMNL_reTerminal_E1003.spiffs.bin").write_bytes(b"littlefs")
            firmware_release.write_manifest(
                "TRMNL_reTerminal_E1003",
                "1.8.7",
                firmware_dir,
                spiffs_offset=0x620000,
                boot_app0_offset=0x13000,
                app_offset=0x20000,
            )

            manifest = json.loads((firmware_dir / "manifest.json").read_text(encoding="utf-8"))
            parts = manifest["builds"][0]["parts"]
            offsets = {path_without_query(part["path"]): part["offset"] for part in parts}

            self.assertEqual(manifest["flashSize"], "keep")
            self.assertEqual(len(parts), 5)
            self.assertEqual(offsets["boot_app0.bin"], 0x13000)
            self.assertEqual(offsets["TRMNL_reTerminal_E1003.ino.bin"], 0x20000)
            self.assertEqual(offsets["TRMNL_reTerminal_E1003.spiffs.bin"], 0x620000)

    def test_manifest_adds_file_hash_queries(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            firmware_dir = Path(temp_dir)
            firmware_id = "TRMNL_reTerminal_E1003"
            create_manifest_artifacts(firmware_dir, firmware_id)
            firmware_release.write_manifest(firmware_id, "1.8.7", firmware_dir)

            manifest = json.loads((firmware_dir / "manifest.json").read_text(encoding="utf-8"))
            parts = manifest["builds"][0]["parts"]

            self.assertTrue(all("?sha256=" in part["path"] for part in parts))
            self.assertTrue(
                any(part["path"].startswith(f"{firmware_id}.ino.bin?sha256=") for part in parts)
            )

    def test_trmnl_e1003_manifest_includes_filesystem_image(self) -> None:
        target = next(
            target
            for target in firmware_release.FIRMWARE_TARGETS
            if target.id == "TRMNL_reTerminal_E1003"
        )
        self.assertEqual(target.spiffs_offset, 0x620000)

        with tempfile.TemporaryDirectory() as temp_dir:
            firmware_dir = Path(temp_dir)
            create_manifest_artifacts(firmware_dir, target.id)
            (firmware_dir / "TRMNL_reTerminal_E1003.spiffs.bin").write_bytes(b"littlefs")
            firmware_release.write_manifest(
                target.id,
                "1.8.7",
                firmware_dir,
                target.spiffs_offset,
                target.boot_app0_offset,
                target.app_offset,
            )

            manifest = json.loads((firmware_dir / "manifest.json").read_text(encoding="utf-8"))
            parts = manifest["builds"][0]["parts"]
            offsets = {path_without_query(part["path"]): part["offset"] for part in parts}

            self.assertEqual(offsets["TRMNL_reTerminal_E1003.spiffs.bin"], 0x620000)

    def test_missing_artifact_parts_requires_filesystem_image(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            artifact_dir = Path(temp_dir)
            firmware_id = "TRMNL_reTerminal_E1003"
            (artifact_dir / f"{firmware_id}.ino.bootloader.bin").write_bytes(b"bootloader")
            (artifact_dir / f"{firmware_id}.ino.partitions.bin").write_bytes(b"partitions")
            (artifact_dir / f"{firmware_id}.ino.bin").write_bytes(b"app")
            (artifact_dir / "boot_app0.bin").write_bytes(b"boot_app0")

            missing = firmware_release.missing_artifact_parts(
                artifact_dir,
                firmware_id,
                require_spiffs=True,
            )

            self.assertEqual(missing, [f"{firmware_id}.spiffs.bin"])


class ExternalFirmwareTest(unittest.TestCase):
    def test_photoframe_targets_registered_for_expected_devices(self) -> None:
        external = {item.id: item for item in firmware_release.EXTERNAL_FIRMWARE}
        self.assertIn("PhotoFrame_reTerminal_E1002", external)
        self.assertIn("PhotoFrame_reTerminal_E1004", external)
        self.assertEqual(external["PhotoFrame_reTerminal_E1002"].devices, ("E1002",))
        self.assertEqual(external["PhotoFrame_reTerminal_E1004"].devices, ("E1004",))
        self.assertTrue(
            all(item.group == "community" for item in firmware_release.EXTERNAL_FIRMWARE)
        )

    def test_external_manifest_is_single_merged_part_at_zero(self) -> None:
        external = firmware_release.ExternalFirmware(
            id="PhotoFrame_reTerminal_E1002",
            version="2.8.0",
            url="https://example.test/photoframe-firmware-e1002-merged.bin",
            devices=("E1002",),
            title="ESP32 PhotoFrame for E1002",
        )

        def fake_download(url: str, destination: Path) -> None:
            destination.write_bytes(b"merged-firmware-image")

        with tempfile.TemporaryDirectory() as temp_dir, patch.object(
            firmware_release, "download_binary", side_effect=fake_download
        ):
            destination = Path(temp_dir) / external.id / external.version
            firmware_release.write_external_manifest(external, destination)

            manifest = json.loads((destination / "manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(manifest["version"], "2.8.0")
            self.assertTrue(manifest["new_install_prompt_erase"])
            builds = manifest["builds"]
            self.assertEqual(builds[0]["chipFamily"], "ESP32-S3")
            parts = builds[0]["parts"]
            self.assertEqual(len(parts), 1)
            self.assertEqual(parts[0]["offset"], 0)
            self.assertTrue(
                path_without_query(parts[0]["path"]).endswith("-merged.bin")
            )
            self.assertIn("?sha256=", parts[0]["path"])
            self.assertTrue(
                (destination / "photoframe-firmware-e1002-merged.bin").exists()
            )

    def test_publish_external_firmware_is_idempotent(self) -> None:
        external = (
            firmware_release.ExternalFirmware(
                id="PhotoFrame_reTerminal_E1002",
                version="2.8.0",
                url="https://example.test/e1002-merged.bin",
                devices=("E1002",),
            ),
        )

        calls = {"count": 0}

        def fake_download(url: str, destination: Path) -> None:
            calls["count"] += 1
            destination.write_bytes(b"merged")

        with tempfile.TemporaryDirectory() as temp_dir, patch.object(
            firmware_release, "download_binary", side_effect=fake_download
        ):
            firmware_root = Path(temp_dir)

            first = firmware_release.publish_external_firmware(firmware_root, external)
            self.assertEqual(first, {"PhotoFrame_reTerminal_E1002": "2.8.0"})

            second = firmware_release.publish_external_firmware(firmware_root, external)
            self.assertEqual(second, {})
            self.assertEqual(calls["count"], 1)

            versions = firmware_release.write_versions_json(firmware_root)
            self.assertEqual(versions["PhotoFrame_reTerminal_E1002"], ["2.8.0"])

    def test_publish_external_firmware_fails_when_download_fails(self) -> None:
        external = (
            firmware_release.ExternalFirmware(
                id="PhotoFrame_reTerminal_E1002",
                version="2.8.0",
                url="https://example.test/e1002-merged.bin",
                devices=("E1002",),
            ),
        )

        with tempfile.TemporaryDirectory() as temp_dir, patch.object(
            firmware_release,
            "download_binary",
            side_effect=RuntimeError("download failed"),
        ):
            with self.assertRaises(RuntimeError):
                firmware_release.publish_external_firmware(Path(temp_dir), external)

    def test_catalog_lists_external_firmware_as_community(self) -> None:
        external = (
            firmware_release.ExternalFirmware(
                id="PhotoFrame_reTerminal_E1004",
                version="2.8.0",
                url="https://example.test/e1004-merged.bin",
                devices=("E1004",),
                title="ESP32 PhotoFrame for E1004",
            ),
        )

        def fake_download(url: str, destination: Path) -> None:
            destination.write_bytes(b"merged")

        with tempfile.TemporaryDirectory() as temp_dir, patch.object(
            firmware_release, "download_binary", side_effect=fake_download
        ):
            firmware_root = Path(temp_dir)
            firmware_release.publish_external_firmware(firmware_root, external)
            firmware_release.write_catalog_json(firmware_root, [], external)

            catalog = json.loads(
                (firmware_root / "catalog.json").read_text(encoding="utf-8")
            )
            entry = next(
                item
                for item in catalog["firmware"]
                if item["id"] == "PhotoFrame_reTerminal_E1004"
            )
            self.assertEqual(entry["group"], "community")
            self.assertEqual(entry["compatible"], ["E1004"])
            self.assertFalse(entry["autoDiscovered"])


if __name__ == "__main__":
    unittest.main()

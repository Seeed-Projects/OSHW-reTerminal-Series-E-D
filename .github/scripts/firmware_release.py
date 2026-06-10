#!/usr/bin/env python3
"""Plan and prepare firmware releases for GitHub Actions.

计划并准备 GitHub Actions 的固件发布内容。
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


DATE_VERSION_RE = re.compile(r"^\d{4}\.\d{2}\.\d{2}(?:\.\d+)?$")
EXAMPLE_GROUPS = {"base", "official", "community"}
SOURCE_REPO_DIR = Path(__file__).resolve().parents[2]


@dataclass(frozen=True)
class FirmwareTarget:
    """One flashable firmware target.

    单个可烧录固件目标。
    """

    id: str
    path: str
    tool: str = "arduino"
    devices: tuple[str, ...] = ("E1001", "E1002", "E1003", "E1004")
    needs_gfx: bool = False
    needs_gxepd2: bool = False
    needs_sht4x: bool = False
    needs_open_font_render: bool = False
    spiffs_image_size: str = ""
    spiffs_offset: int = 0
    build_flags: str = ""
    pio_env: str = ""
    auto_discovered: bool = False
    title: str = ""
    group: str = ""

    def to_matrix(self) -> dict[str, Any]:
        return {
            "name": self.id,
            "path": self.path,
            "needs_gfx": self.needs_gfx,
            "needs_gxepd2": self.needs_gxepd2,
            "needs_sht4x": self.needs_sht4x,
            "needs_open_font_render": self.needs_open_font_render,
            "spiffs_image_size": self.spiffs_image_size,
            "spiffs_offset": self.spiffs_offset,
            "build_flags": self.build_flags,
            "pio_env": self.pio_env,
        }

    def to_catalog(self) -> dict[str, Any]:
        item = {
            "id": self.id,
            "name": self.title or self.id.replace("_", " "),
            "path": self.path,
            "tool": self.tool,
            "compatible": list(self.devices),
            "autoDiscovered": self.auto_discovered,
        }
        if self.group:
            item["group"] = self.group
        return item


FIRMWARE_TARGETS: tuple[FirmwareTarget, ...] = (
    FirmwareTarget("RTC_PCF8563", "examples/base/RTC_PCF8563", title="RTC PCF8563"),
    FirmwareTarget("LowPower_DeepSleep", "examples/base/LowPower_DeepSleep", title="Low Power Deep Sleep"),
    FirmwareTarget(
        "MicRecordToSD_E1001",
        "examples/base/MicRecordToSD",
        devices=("E1001", "E1002"),
        build_flags="-DDEVICE_E1001_E1002",
        title="Mic Record to SD for E1001/E1002",
    ),
    FirmwareTarget(
        "MicRecordToSD_E1003",
        "examples/base/MicRecordToSD",
        devices=("E1003",),
        build_flags="-DDEVICE_E1003",
        title="Mic Record to SD for E1003",
    ),
    FirmwareTarget(
        "E1003_TouchDraw",
        "examples/base/E1003_TouchDraw",
        devices=("E1003",),
        needs_gfx=True,
        title="E1003 Touch Draw",
    ),
    FirmwareTarget(
        "LED_Control_E1001",
        "examples/base/LED_Control",
        devices=("E1001", "E1002"),
        build_flags="-DDEVICE_MODEL=1001",
        title="LED Control for E1001/E1002",
    ),
    FirmwareTarget(
        "LED_Control_E1003",
        "examples/base/LED_Control",
        devices=("E1003",),
        build_flags="-DDEVICE_MODEL=1003",
        title="LED Control for E1003",
    ),
    FirmwareTarget(
        "LED_Control_E1004",
        "examples/base/LED_Control",
        devices=("E1004",),
        build_flags="-DDEVICE_MODEL=1004",
        title="LED Control for E1004",
    ),
    FirmwareTarget("Buzzer_Control", "examples/base/Buzzer_Control", title="Buzzer Control"),
    FirmwareTarget("Buzzer_Music", "examples/base/Buzzer_Music", title="Buzzer Music"),
    FirmwareTarget("UserButtons", "examples/base/UserButtons", title="User Buttons"),
    FirmwareTarget("Battery_Monitor", "examples/base/Battery_Monitor", title="Battery Monitor"),
    FirmwareTarget("MicroSD_ListFiles", "examples/base/MicroSD_ListFiles", title="MicroSD List Files"),
    FirmwareTarget(
        "GxEPD2_reTerminal_E1001",
        "examples/base/GxEPD2_reTerminal_E1001",
        devices=("E1001",),
        needs_gxepd2=True,
        title="GxEPD2 reTerminal E1001",
    ),
    FirmwareTarget(
        "GxEPD2_reTerminal_E1002",
        "examples/base/GxEPD2_reTerminal_E1002",
        devices=("E1002",),
        needs_gxepd2=True,
        title="GxEPD2 reTerminal E1002",
    ),
    FirmwareTarget(
        "GxEPD2_reTerminal_E1003",
        "examples/base/GxEPD2_reTerminal_E1003",
        devices=("E1003",),
        needs_gxepd2=True,
        title="GxEPD2 reTerminal E1003",
    ),
    FirmwareTarget(
        "GxEPD2_reTerminal_E1004",
        "examples/base/GxEPD2_reTerminal_E1004",
        devices=("E1004",),
        needs_gxepd2=True,
        title="GxEPD2 reTerminal E1004",
    ),
    FirmwareTarget(
        "GxEPD2_reTerminal_E1001_Gray4",
        "examples/base/GxEPD2_reTerminal_E1001_Gray4",
        devices=("E1001",),
        needs_gxepd2=True,
        title="GxEPD2 reTerminal E1001 Gray4",
    ),
    FirmwareTarget(
        "Seeed_GFX_E1001_Gray4",
        "examples/base/Seeed_GFX_E1001_Gray4",
        devices=("E1001",),
        needs_gfx=True,
        title="Seeed GFX E1001 Gray4",
    ),
    FirmwareTarget(
        "GxEPD2_reTerminal_E1003_Gray16",
        "examples/base/GxEPD2_reTerminal_E1003_Gray16",
        devices=("E1003",),
        needs_gxepd2=True,
        title="GxEPD2 reTerminal E1003 Gray16",
    ),
    FirmwareTarget(
        "SHT4x_Sensor",
        "examples/base/SHT4x_Sensor",
        needs_sht4x=True,
        title="SHT4x Sensor",
    ),
    FirmwareTarget(
        "E1001_ChineseTextDemo",
        "examples/base/E1001_ChineseTextDemo",
        devices=("E1001",),
        needs_gfx=True,
        needs_open_font_render=True,
        spiffs_image_size="0x4E0000",
        spiffs_offset=0x310000,
        title="E1001 Chinese Text Demo",
    ),
    FirmwareTarget(
        "E1002_ChineseTextDemo",
        "examples/base/E1002_ChineseTextDemo",
        devices=("E1002",),
        needs_gfx=True,
        needs_open_font_render=True,
        spiffs_image_size="0x4E0000",
        spiffs_offset=0x310000,
        title="E1002 Chinese Text Demo",
    ),
    FirmwareTarget(
        "E1003_ChineseTextDemo",
        "examples/base/E1003_ChineseTextDemo",
        devices=("E1003",),
        needs_gfx=True,
        needs_open_font_render=True,
        spiffs_image_size="0x4E0000",
        spiffs_offset=0x310000,
        title="E1003 Chinese Text Demo",
    ),
    FirmwareTarget(
        "E1004_ChineseTextDemo",
        "examples/base/E1004_ChineseTextDemo",
        devices=("E1004",),
        needs_gfx=True,
        needs_open_font_render=True,
        spiffs_image_size="0x4E0000",
        spiffs_offset=0x310000,
        title="E1004 Chinese Text Demo",
    ),
    FirmwareTarget(
        "SD_ImagePipeline_E1001_BW",
        "examples/base/SD_ImagePipeline_E1001_BW",
        devices=("E1001",),
        needs_gfx=True,
        title="SD Image Pipeline E1001 BW",
    ),
    FirmwareTarget(
        "SD_ImagePipeline_E1001_Gray4",
        "examples/base/SD_ImagePipeline_E1001_Gray4",
        devices=("E1001",),
        needs_gfx=True,
        title="SD Image Pipeline E1001 Gray4",
    ),
    FirmwareTarget(
        "SD_ImagePipeline_E1002",
        "examples/base/SD_ImagePipeline_E1002",
        devices=("E1002",),
        needs_gfx=True,
        title="SD Image Pipeline E1002",
    ),
    FirmwareTarget(
        "SD_ImagePipeline_E1003",
        "examples/base/SD_ImagePipeline_E1003",
        devices=("E1003",),
        needs_gfx=True,
        title="SD Image Pipeline E1003",
    ),
    FirmwareTarget(
        "SD_ImagePipeline_E1004",
        "examples/base/SD_ImagePipeline_E1004",
        devices=("E1004",),
        needs_gfx=True,
        title="SD Image Pipeline E1004",
    ),
    FirmwareTarget(
        "ePaper_VoiceMemo_E1001",
        "examples/community/ePaper-Voice-Memo",
        tool="platformio",
        devices=("E1001",),
        pio_env="reterminal_e1001",
        title="Voice Memo Reminder for E1001",
    ),
    FirmwareTarget(
        "ePaper_VoiceMemo_E1002",
        "examples/community/ePaper-Voice-Memo",
        tool="platformio",
        devices=("E1002",),
        pio_env="reterminal_e1002",
        title="Voice Memo Reminder for E1002",
    ),
    FirmwareTarget(
        "ePaper_VoiceMemo_E1003",
        "examples/community/ePaper-Voice-Memo",
        tool="platformio",
        devices=("E1003",),
        pio_env="reterminal_e1003",
        title="Voice Memo Reminder for E1003",
    ),
    FirmwareTarget(
        "ePaper_VoiceMemo_E1001_ZH",
        "examples/community/ePaper-Voice-Memo",
        tool="platformio",
        devices=("E1001",),
        pio_env="reterminal_e1001_zh",
        title="Voice Memo Reminder Chinese for E1001",
    ),
    FirmwareTarget(
        "ePaper_VoiceMemo_E1002_ZH",
        "examples/community/ePaper-Voice-Memo",
        tool="platformio",
        devices=("E1002",),
        pio_env="reterminal_e1002_zh",
        title="Voice Memo Reminder Chinese for E1002",
    ),
    FirmwareTarget(
        "ePaper_VoiceMemo_E1003_ZH",
        "examples/community/ePaper-Voice-Memo",
        tool="platformio",
        devices=("E1003",),
        pio_env="reterminal_e1003_zh",
        title="Voice Memo Reminder Chinese for E1003",
    ),
)


@dataclass
class ReleasePlan:
    changed_files: list[str]
    all_targets: list[FirmwareTarget]
    changed_targets: list[FirmwareTarget] = field(default_factory=list)

    def as_json(self) -> dict[str, Any]:
        return {
            "firmware_changed": bool(self.changed_targets),
            "changed_files": self.changed_files,
            "changed_firmware": [target.to_catalog() for target in self.changed_targets],
            "all_firmware": [target.to_catalog() for target in self.all_targets],
            "arduino_matrix": {
                "include": [
                    target.to_matrix()
                    for target in self.changed_targets
                    if target.tool == "arduino"
                ]
            },
            "pio_matrix": {
                "include": [
                    target.to_matrix()
                    for target in self.changed_targets
                    if target.tool == "platformio"
                ]
            },
        }


def normalize_path(value: str) -> str:
    return value.strip().replace("\\", "/").lstrip("./")


def is_under(path: str, directory: str) -> bool:
    normalized = normalize_path(path)
    prefix = normalize_path(directory).rstrip("/") + "/"
    return normalized == normalize_path(directory) or normalized.startswith(prefix)


def parse_conventional_example(path: str) -> tuple[str, str, str] | None:
    """Return group, folder, and path for conventional Arduino examples.

    返回常规 Arduino 示例的分组、文件夹名和路径。
    """

    parts = normalize_path(path).split("/")
    if len(parts) < 3 or parts[0] != "examples":
        return None

    group = "base"
    folder_index = 1
    ino_index = 2
    if parts[1] in EXAMPLE_GROUPS:
        if len(parts) < 4:
            return None
        group = parts[1]
        folder_index = 2
        ino_index = 3

    folder = parts[folder_index]
    if parts[ino_index] != f"{folder}.ino":
        return None

    if group == "base" and folder_index == 1:
        example_path = f"examples/base/{folder}"
    else:
        example_path = f"examples/{group}/{folder}"
    return group, folder, example_path


def discover_default_targets(changed_files: list[str], known_targets: list[FirmwareTarget]) -> list[FirmwareTarget]:
    """Create default Arduino targets for new conventional examples.

    为符合约定的新 Arduino 示例创建默认构建目标。
    """

    known_paths = {normalize_path(target.path) for target in known_targets}
    known_ids = {target.id for target in known_targets}
    discovered: list[FirmwareTarget] = []
    seen: set[str] = set()

    for changed_file in changed_files:
        parsed = parse_conventional_example(changed_file)
        if not parsed:
            continue
        group, folder, example_path = parsed
        if example_path in known_paths or folder in known_ids or folder in seen:
            continue
        discovered.append(
            FirmwareTarget(
                id=folder,
                path=example_path,
                title=folder.replace("_", " "),
                auto_discovered=True,
                group=group,
            )
        )
        seen.add(folder)

    return discovered


def build_plan(changed_files: list[str]) -> ReleasePlan:
    known_targets = list(FIRMWARE_TARGETS)
    all_targets = known_targets + discover_default_targets(changed_files, known_targets)
    changed_targets = [
        target
        for target in all_targets
        if any(is_under(changed_file, target.path) for changed_file in changed_files)
    ]
    return ReleasePlan(
        changed_files=changed_files,
        all_targets=all_targets,
        changed_targets=changed_targets,
    )


def write_github_outputs(path: Path | None, values: dict[str, str]) -> None:
    if not path:
        return
    if isinstance(path, str):
        path = Path(path)
    with path.open("a", encoding="utf-8") as output:
        for key, value in values.items():
            output.write(f"{key}={value}\n")


def version_sort_key(version: str) -> tuple[int, int, int, int]:
    parts = version.split(".")
    suffix = int(parts[3]) if len(parts) > 3 and parts[3].isdigit() else 0
    return (int(parts[0]), int(parts[1]), int(parts[2]), suffix)


def date_versions(firmware_dir: Path) -> list[str]:
    if not firmware_dir.exists():
        return []
    versions = [
        entry.name
        for entry in firmware_dir.iterdir()
        if entry.is_dir()
        and DATE_VERSION_RE.match(entry.name)
        and (entry / "manifest.json").exists()
    ]
    return sorted(versions, key=version_sort_key, reverse=True)


def next_date_version(existing_versions: list[str], today: str) -> str:
    same_day = [
        version for version in existing_versions
        if version == today or version.startswith(f"{today}.")
    ]
    if not same_day:
        return today
    suffixes = [0]
    for version in same_day:
        parts = version.split(".")
        suffixes.append(int(parts[3]) if len(parts) > 3 and parts[3].isdigit() else 0)
    return f"{today}.{max(suffixes) + 1}"


def copy_web_assets(web_dir: Path, pages_dir: Path) -> None:
    pages_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(web_dir / "index.html", pages_dir / "index.html")
    for root_file in ("robots.txt", "sitemap.xml"):
        src = web_dir / root_file
        if src.exists():
            shutil.copy2(src, pages_dir / root_file)
    for folder in ("css", "js", "assets"):
        source = web_dir / folder
        if not source.exists():
            continue
        destination = pages_dir / folder
        shutil.copytree(source, destination, dirs_exist_ok=True)


def copy_firmware_artifact(source_dir: Path, firmware_id: str, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    parts = {
        "*.ino.bootloader.bin": f"{firmware_id}.ino.bootloader.bin",
        "*.ino.partitions.bin": f"{firmware_id}.ino.partitions.bin",
        "*.ino.bin": f"{firmware_id}.ino.bin",
    }
    for pattern, filename in parts.items():
        matches = sorted(source_dir.glob(pattern))
        if not matches:
            raise FileNotFoundError(f"Missing {pattern} for {firmware_id}")
        shutil.copy2(matches[0], destination / filename)

    boot_app0 = source_dir / "boot_app0.bin"
    if not boot_app0.exists():
        raise FileNotFoundError(f"Missing boot_app0.bin for {firmware_id}")
    shutil.copy2(boot_app0, destination / "boot_app0.bin")

    spiffs = source_dir / f"{firmware_id}.spiffs.bin"
    if spiffs.exists():
        shutil.copy2(spiffs, destination / f"{firmware_id}.spiffs.bin")


def missing_artifact_parts(source_dir: Path, firmware_id: str) -> list[str]:
    """Return required firmware artifact patterns that are missing.

    返回缺失的必要固件产物模式。
    """

    missing: list[str] = []
    for pattern in ("*.ino.bootloader.bin", "*.ino.partitions.bin", "*.ino.bin"):
        if not sorted(source_dir.glob(pattern)):
            missing.append(pattern)
    if not (source_dir / "boot_app0.bin").exists():
        missing.append("boot_app0.bin")
    return missing


def write_manifest(
    firmware_id: str,
    version: str,
    destination: Path,
    spiffs_offset: int = 0,
) -> None:
    parts = [
        {"path": f"{firmware_id}.ino.bootloader.bin", "offset": 0},
        {"path": f"{firmware_id}.ino.partitions.bin", "offset": 32768},
        {"path": "boot_app0.bin", "offset": 57344},
        {"path": f"{firmware_id}.ino.bin", "offset": 65536},
    ]
    spiffs = destination / f"{firmware_id}.spiffs.bin"
    if spiffs.exists():
        if not spiffs_offset:
            raise ValueError(f"Missing SPIFFS offset for {firmware_id}")
        parts.append({"path": spiffs.name, "offset": spiffs_offset})

    manifest = {
        "name": firmware_id,
        "version": version,
        "new_install_prompt_erase": True,
        "builds": [
            {
                "chipFamily": "ESP32-S3",
                "parts": parts,
            }
        ],
    }
    (destination / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n",
        encoding="utf-8",
    )


def migrate_legacy_latest(firmware_root: Path, today: str) -> None:
    """Copy legacy latest-only firmware into a date version once.

    将只有 latest 的旧固件复制成一次日期版本。
    """

    if not firmware_root.exists():
        return
    for firmware_dir in sorted(entry for entry in firmware_root.iterdir() if entry.is_dir()):
        if date_versions(firmware_dir):
            continue
        latest_dir = firmware_dir / "latest"
        if not (latest_dir / "manifest.json").exists():
            continue
        version = next_date_version([], today)
        version_dir = firmware_dir / version
        if version_dir.exists():
            continue
        shutil.copytree(latest_dir, version_dir)
        write_manifest(firmware_dir.name, version, version_dir)


def write_versions_json(firmware_root: Path) -> dict[str, list[str]]:
    versions: dict[str, list[str]] = {}
    firmware_root.mkdir(parents=True, exist_ok=True)
    for firmware_dir in sorted(entry for entry in firmware_root.iterdir() if entry.is_dir()):
        entries = date_versions(firmware_dir)
        if entries:
            versions[firmware_dir.name] = entries
    (firmware_root / "versions.json").write_text(
        json.dumps(versions, indent=2) + "\n",
        encoding="utf-8",
    )
    return versions


def write_catalog_json(firmware_root: Path, targets: list[FirmwareTarget]) -> None:
    catalog_items = [target.to_catalog() for target in targets]
    known_ids = {target.id for target in targets}
    for firmware_dir in sorted(entry for entry in firmware_root.iterdir() if entry.is_dir()):
        if firmware_dir.name in known_ids or not date_versions(firmware_dir):
            continue
        catalog_items.append(
            FirmwareTarget(
                id=firmware_dir.name,
                path=f"examples/base/{firmware_dir.name}",
                title=firmware_dir.name.replace("_", " "),
                auto_discovered=True,
            ).to_catalog()
        )
    catalog = {"firmware": catalog_items}
    (firmware_root / "catalog.json").write_text(
        json.dumps(catalog, indent=2) + "\n",
        encoding="utf-8",
    )


def copy_release_assets(firmware_root: Path, versions: dict[str, list[str]], release_dir: Path) -> None:
    release_dir.mkdir(parents=True, exist_ok=True)
    for firmware_id, firmware_versions in sorted(versions.items()):
        if not firmware_versions:
            continue
        current_version = firmware_versions[0]
        source_dir = firmware_root / firmware_id / current_version
        for artifact in sorted(source_dir.glob("*.bin")):
            destination = release_dir / f"{firmware_id}_{current_version}_{artifact.name}"
            shutil.copy2(artifact, destination)


def matching_release_tags(base: str, repo_dir: Path) -> set[str]:
    tags: set[str] = set()
    # Read both local and remote tags so CI can reserve the next same-day release.
    # 同时读取本地和远端 tag，用于在 CI 中生成同一天的下一个发布号。
    commands = [
        ["git", "-C", str(repo_dir), "tag", "-l", f"{base}*"],
        ["git", "-C", str(repo_dir), "ls-remote", "--tags", "origin", f"refs/tags/{base}*"],
    ]
    for command in commands:
        try:
            completed = subprocess.run(
                command,
                check=True,
                capture_output=True,
                text=True,
            )
        except (subprocess.CalledProcessError, FileNotFoundError):
            continue
        for line in completed.stdout.splitlines():
            value = line.strip()
            if not value:
                continue
            if "\t" in value:
                value = value.rsplit("\t", 1)[1]
            value = value.removeprefix("refs/tags/").removesuffix("^{}")
            tags.add(value)
    return tags


def next_release_tag(today: str, repo_dir: Path = SOURCE_REPO_DIR) -> str:
    base = f"fw-{today}"
    existing = matching_release_tags(base, repo_dir)
    if base not in existing:
        return base
    suffixes = [0]
    for tag in existing:
        if tag.startswith(f"{base}."):
            suffix = tag.removeprefix(f"{base}.")
            if suffix.isdigit():
                suffixes.append(int(suffix))
    return f"{base}.{max(suffixes) + 1}"


def release_title_from_tag(release_tag: str) -> str:
    return f"Firmware {release_tag.removeprefix('fw-')}"


def write_release_notes(
    release_notes: Path,
    changed_versions: dict[str, str],
    versions: dict[str, list[str]],
) -> None:
    lines = [
        "## Updated in this release",
        "",
        "| Firmware | New version |",
        "|:--|:--|",
    ]
    if changed_versions:
        for firmware_id, version in sorted(changed_versions.items()):
            lines.append(f"| {firmware_id} | {version} |")
    else:
        lines.append("| None | N/A |")
    lines.extend([
        "",
        "## Included latest firmware",
        "",
        "| Firmware | Included version |",
        "|:--|:--|",
    ])
    for firmware_id, firmware_versions in sorted(versions.items()):
        if firmware_versions:
            lines.append(f"| {firmware_id} | {firmware_versions[0]} |")
    lines.append("")
    release_notes.write_text("\n".join(lines), encoding="utf-8")


def prepare_pages(
    plan: ReleasePlan,
    pages_dir: Path,
    artifacts_dir: Path,
    web_dir: Path,
    release_assets_dir: Path,
    release_notes: Path,
    today: str,
) -> dict[str, str]:
    copy_web_assets(web_dir, pages_dir)
    firmware_root = pages_dir / "firmware"
    firmware_root.mkdir(parents=True, exist_ok=True)
    migrate_legacy_latest(firmware_root, today)

    changed_versions: dict[str, str] = {}
    skipped_targets: dict[str, str] = {}
    for target in plan.changed_targets:
        artifact_dir = artifacts_dir / f"firmware-{target.id}"
        if not artifact_dir.exists():
            skipped_targets[target.id] = "missing artifact directory"
            continue
        missing_parts = missing_artifact_parts(artifact_dir, target.id)
        if missing_parts:
            skipped_targets[target.id] = f"missing {', '.join(missing_parts)}"
            continue
        firmware_dir = firmware_root / target.id
        version = next_date_version(date_versions(firmware_dir), today)
        destination = firmware_dir / version
        copy_firmware_artifact(artifact_dir, target.id, destination)
        write_manifest(target.id, version, destination, target.spiffs_offset)
        changed_versions[target.id] = version
        print(f"Updated firmware: {target.id} -> {version}")

    for firmware_id, reason in sorted(skipped_targets.items()):
        print(f"Skipped firmware: {firmware_id} ({reason})")

    versions = write_versions_json(firmware_root)
    write_catalog_json(firmware_root, plan.all_targets)
    copy_release_assets(firmware_root, versions, release_assets_dir)
    write_release_notes(release_notes, changed_versions, versions)
    release_ready = bool(plan.changed_targets) and not skipped_targets

    release_tag = next_release_tag(today)
    return {
        "release_tag": release_tag,
        "release_title": release_title_from_tag(release_tag),
        "firmware_changed": "true" if changed_versions else "false",
        "release_ready": "true" if release_ready else "false",
        "skipped_firmware": ",".join(sorted(skipped_targets)),
    }


def load_plan(path: Path) -> ReleasePlan:
    data = json.loads(path.read_text(encoding="utf-8"))
    targets_by_id = {
        target.id: target
        for target in list(FIRMWARE_TARGETS)
        + [
            FirmwareTarget(
                id=item["id"],
                path=item["path"],
                tool=item.get("tool", "arduino"),
                devices=tuple(item.get("compatible", ["E1001", "E1002", "E1003", "E1004"])),
                auto_discovered=bool(item.get("autoDiscovered")),
                title=item.get("name", ""),
                group=item.get("group", ""),
            )
            for item in data.get("all_firmware", [])
            if item.get("autoDiscovered")
        ]
    }
    all_targets = [targets_by_id[item["id"]] for item in data.get("all_firmware", [])]
    changed_targets = [targets_by_id[item["id"]] for item in data.get("changed_firmware", [])]
    return ReleasePlan(
        changed_files=list(data.get("changed_files", [])),
        all_targets=all_targets,
        changed_targets=changed_targets,
    )


def command_plan(args: argparse.Namespace) -> None:
    changed_files = [normalize_path(path) for path in args.changed_file if path.strip()]
    plan = build_plan(changed_files)
    payload = plan.as_json()
    args.output_file.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    arduino_matrix = payload["arduino_matrix"]
    pio_matrix = payload["pio_matrix"]
    write_github_outputs(
        args.github_output,
        {
            "firmware_changed": "true" if payload["firmware_changed"] else "false",
            "has_arduino": "true" if arduino_matrix["include"] else "false",
            "has_pio": "true" if pio_matrix["include"] else "false",
            "arduino_matrix": json.dumps(arduino_matrix, separators=(",", ":")),
            "pio_matrix": json.dumps(pio_matrix, separators=(",", ":")),
        },
    )
    print(json.dumps(payload, indent=2))


def command_prepare_pages(args: argparse.Namespace) -> None:
    plan = load_plan(args.plan_file)
    today = args.today or datetime.now(timezone.utc).strftime("%Y.%m.%d")
    outputs = prepare_pages(
        plan=plan,
        pages_dir=args.pages_dir,
        artifacts_dir=args.artifacts_dir,
        web_dir=args.web_dir,
        release_assets_dir=args.release_assets_dir,
        release_notes=args.release_notes,
        today=today,
    )
    write_github_outputs(args.github_output, outputs)
    print(json.dumps(outputs, indent=2))


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(description="Plan and prepare firmware releases.")
    subparsers = root.add_subparsers(dest="command", required=True)

    plan = subparsers.add_parser("plan", help="Build a firmware release plan.")
    plan.add_argument("--changed-file", action="append", default=[])
    plan.add_argument("--output-file", type=Path, required=True)
    plan.add_argument("--github-output", type=Path, default=os.environ.get("GITHUB_OUTPUT"))
    plan.set_defaults(func=command_plan)

    prepare = subparsers.add_parser("prepare-pages", help="Prepare gh-pages content and release assets.")
    prepare.add_argument("--plan-file", type=Path, required=True)
    prepare.add_argument("--pages-dir", type=Path, required=True)
    prepare.add_argument("--artifacts-dir", type=Path, required=True)
    prepare.add_argument("--web-dir", type=Path, required=True)
    prepare.add_argument("--release-assets-dir", type=Path, required=True)
    prepare.add_argument("--release-notes", type=Path, required=True)
    prepare.add_argument("--today")
    prepare.add_argument("--github-output", type=Path, default=os.environ.get("GITHUB_OUTPUT"))
    prepare.set_defaults(func=command_prepare_pages)

    return root


def main() -> None:
    args = parser().parse_args()
    args.func(args)


if __name__ == "__main__":
    main()

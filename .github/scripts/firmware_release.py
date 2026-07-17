#!/usr/bin/env python3
"""Plan and prepare firmware releases for GitHub Actions.

计划并准备 GitHub Actions 的固件发布内容。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import urllib.request
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable


FIRMWARE_VERSION_RE = re.compile(r"^(?:\d{4}\.\d{2}\.\d{2}(?:\.\d+)?|\d+\.\d+\.\d+)$")
EXAMPLE_GROUPS = {"base", "official", "community"}
SOURCE_REPO_DIR = Path(__file__).resolve().parents[2]


def cache_busted_path(path: Path, display_name: str | None = None) -> str:
    """Return a relative firmware path with a content hash query.

    返回带内容哈希查询参数的相对固件路径。
    """

    digest = hashlib.sha256(path.read_bytes()).hexdigest()[:16]
    name = display_name or path.name
    return f"{name}?sha256={digest}"


@dataclass(frozen=True)
class FirmwareTarget:
    """One flashable firmware target.

    单个可烧录固件目标。
    """

    id: str
    path: str
    tool: str = "arduino"
    # Target MCU family: "esp32" builds .bin sets, "nrf52" builds a .uf2.
    # 目标芯片家族："esp32" 产出 .bin 组合，"nrf52" 产出 .uf2。
    chip: str = "esp32"
    devices: tuple[str, ...] = ("E1001", "E1002", "E1003", "E1004")
    needs_gfx: bool = False
    needs_gxepd2: bool = False
    needs_sht4x: bool = False
    needs_open_font_render: bool = False
    spiffs_image_size: str = ""
    spiffs_offset: int = 0
    filesystem_image_url: str = ""
    include_filesystem: bool = True
    boot_app0_offset: int = 0xE000
    app_offset: int = 0x10000
    flash_size: str = "keep"
    fixed_version: str = ""
    build_flags: str = ""
    # Extra C++ defines passed via compiler.cpp.extra_flags, which keeps the
    # board's own build.extra_flags (USB / chip defines) intact.
    # 通过 compiler.cpp.extra_flags 注入的额外 C++ 宏，
    # 不会覆盖板卡自身的 build.extra_flags（USB / 芯片相关宏）。
    cpp_flags: str = ""
    pio_env: str = ""
    rebuild_triggers: tuple[str, ...] = ()
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
            "filesystem_image_url": self.filesystem_image_url,
            "include_filesystem": self.include_filesystem,
            "fixed_version": self.fixed_version,
            "build_flags": self.build_flags,
            "cpp_flags": self.cpp_flags,
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


# XIAO ePaper DIY Kit combos: one firmware artifact per board + panel pairing.
# Panel ids match web/js/firmwares.js PANELS; the setup id selects the
# Seeed_GFX User_Setup (driver chip + resolution) at compile time.
# XIAO ePaper DIY Kit 组合：每个「板 + 屏」搭配产出一个固件。
# 屏幕 id 与 web/js/firmwares.js 的 PANELS 一致；setup 编号在编译期
# 选择 Seeed_GFX 的 User_Setup（驱动芯片 + 分辨率）。
DIY_PANEL_SETUPS: dict[str, str] = {
    "P075_MONO": "502",
    "P073_SP6": "509",
    "P133_SP6": "510",
    "P103_MONO": "511",
    "P0583_MONO": "503",
    "P0426_MONO": "506",
    "P042_MONO": "507",
    "P029_MONO": "504",
    "P029_QUAD": "512",
    "P0213_MONO": "508",
    "P0213_QUAD": "513",
    "P0154_MONO": "505",
}

DIY_SPI_PANELS: tuple[str, ...] = (
    "P075_MONO",
    "P0583_MONO",
    "P0426_MONO",
    "P042_MONO",
    "P029_MONO",
    "P029_QUAD",
    "P0213_MONO",
    "P0213_QUAD",
    "P0154_MONO",
)

# board id -> (chip family, panels the board can drive)
# 板子 id -> （芯片家族，可驱动的屏幕列表）
DIY_BOARD_PANELS: dict[str, tuple[str, tuple[str, ...]]] = {
    "EE02": ("esp32", ("P133_SP6",)),
    "EE03": ("esp32", ("P103_MONO",)),
    "EE04": ("esp32", ("P073_SP6",) + DIY_SPI_PANELS),
    "EE05": ("esp32", DIY_SPI_PANELS),
    "EN04": ("nrf52", ("P073_SP6",) + DIY_SPI_PANELS),
    "EN05": ("nrf52", DIY_SPI_PANELS),
}


def diy_kit_targets() -> tuple[FirmwareTarget, ...]:
    """Generate one Hello firmware target per DIY Kit board + panel combo.

    为每个 DIY Kit「板 + 屏」组合生成一个 Hello 固件构建目标。
    """

    targets: list[FirmwareTarget] = []
    for board_id, (chip, panel_ids) in DIY_BOARD_PANELS.items():
        for panel_id in panel_ids:
            setup_id = DIY_PANEL_SETUPS[panel_id]
            targets.append(
                FirmwareTarget(
                    id=f"XIAO_EPaper_Hello_{board_id}_{panel_id}",
                    path="examples/base/XIAO_EPaper_Hello",
                    chip=chip,
                    devices=(board_id,),
                    needs_gfx=True,
                    cpp_flags=(
                        f"-DBOARD_SCREEN_COMBO={setup_id} "
                        f"-DUSE_XIAO_EPAPER_DISPLAY_BOARD_{board_id}"
                    ),
                    title=f"Hello ePaper {board_id} + {panel_id}",
                )
            )
    return tuple(targets)


FIRMWARE_TARGETS: tuple[FirmwareTarget, ...] = diy_kit_targets() + (
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
        "TRMNL_reTerminal_E1001",
        "examples/official/TRMNL",
        tool="platformio",
        devices=("E1001",),
        pio_env="seeed_reTerminal_E1001",
        rebuild_triggers=(
            ".github/scripts/firmware_release.py",
            ".github/workflows/build-and-deploy.yml",
        ),
        fixed_version="1.8.10",
        title="TRMNL for reTerminal E1001",
        group="official",
    ),
    FirmwareTarget(
        "TRMNL_reTerminal_E1002",
        "examples/official/TRMNL",
        tool="platformio",
        devices=("E1002",),
        pio_env="seeed_reTerminal_E1002",
        rebuild_triggers=(
            ".github/scripts/firmware_release.py",
            ".github/workflows/build-and-deploy.yml",
        ),
        fixed_version="1.8.10",
        title="TRMNL for reTerminal E1002",
        group="official",
    ),
    FirmwareTarget(
        "TRMNL_reTerminal_E1003",
        "examples/official/TRMNL",
        tool="platformio",
        devices=("E1003",),
        pio_env="TRMNL_X_E1003",
        rebuild_triggers=(
            ".github/scripts/firmware_release.py",
            ".github/workflows/build-and-deploy.yml",
        ),
        boot_app0_offset=0x13000,
        app_offset=0x20000,
        spiffs_offset=0x620000,
        filesystem_image_url="https://trmnl-fw.s3.us-east-2.amazonaws.com/littlefs.bin",
        fixed_version="1.8.10",
        title="TRMNL for reTerminal E1003",
        group="official",
    ),
    FirmwareTarget(
        "LVGL_StatusPanel_E1001",
        "examples/official/LVGLePaperStatusPanel",
        tool="platformio",
        devices=("E1001",),
        pio_env="reterminal_e1001",
        title="LVGL ePaper Status Panel for E1001",
        group="official",
    ),
    FirmwareTarget(
        "LVGL_StatusPanel_E1002",
        "examples/official/LVGLePaperStatusPanel",
        tool="platformio",
        devices=("E1002",),
        pio_env="reterminal_e1002",
        title="LVGL ePaper Status Panel for E1002",
        group="official",
    ),
    FirmwareTarget(
        "LVGL_StatusPanel_E1003",
        "examples/official/LVGLePaperStatusPanel",
        tool="platformio",
        devices=("E1003",),
        pio_env="reterminal_e1003",
        title="LVGL ePaper Status Panel for E1003",
        group="official",
    ),
    FirmwareTarget(
        "LVGL_StatusPanel_E1004",
        "examples/official/LVGLePaperStatusPanel",
        tool="platformio",
        devices=("E1004",),
        pio_env="reterminal_e1004",
        title="LVGL ePaper Status Panel for E1004",
        group="official",
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


@dataclass(frozen=True)
class ExternalFirmware:
    """A browser-flashable firmware whose binary is built and released elsewhere.

    在其他仓库构建和发布的可烧录固件。

    The full-flash merged image (bootloader + partition table + app at offset 0)
    is mirrored onto gh-pages at deploy time so the Firmware Hub can flash it
    same-origin — release-assets.githubusercontent.com does not send CORS headers,
    so the browser cannot fetch the upstream asset directly.

    完整合并镜像（偏移 0 处的 bootloader + 分区表 + 应用）在部署时镜像到 gh-pages，
    以便固件中心同源烧录 —— release-assets.githubusercontent.com 不返回 CORS 头，
    浏览器无法直接抓取上游产物。
    """

    id: str
    # Source GitHub repo + release asset. The version (latest release tag) and
    # the download URL are resolved at publish time, so a new upstream firmware
    # release is picked up automatically without editing this file.
    repo: str
    asset: str
    devices: tuple[str, ...]
    title: str = ""
    group: str = "community"
    chip_family: str = "ESP32-S3"

    def url_for(self, tag: str) -> str:
        """Build the release-asset download URL for a resolved release tag."""
        return f"https://github.com/{self.repo}/releases/download/{tag}/{self.asset}"

    def to_catalog(self) -> dict[str, Any]:
        return {
            "id": self.id,
            "name": self.title or self.id.replace("_", " "),
            "path": "",
            "tool": "external",
            "compatible": list(self.devices),
            "autoDiscovered": False,
            "group": self.group,
        }


# Externally built firmware mirrored into the hub at deploy time. The version is
# resolved automatically from each repo's latest GitHub release, so a new upstream
# release is published on the next deploy without editing this file.
# 在部署时镜像进固件中心的外部固件。版本号在部署时从各仓库的最新 GitHub Release 自动解析，
# 上游发布新版本后，下次部署会自动发布，无需修改此文件。
EXTERNAL_FIRMWARE: tuple[ExternalFirmware, ...] = (
    ExternalFirmware(
        id="PhotoFrame_reTerminal_E1002",
        repo="aitjcize/esp32-photoframe",
        asset="photoframe-firmware-seeedstudio_reterminal_e1002-merged.bin",
        devices=("E1002",),
        title="ESP32 PhotoFrame for E1002",
    ),
    ExternalFirmware(
        id="PhotoFrame_reTerminal_E1003",
        repo="aitjcize/esp32-photoframe",
        asset="photoframe-firmware-seeedstudio_reterminal_e1003-merged.bin",
        devices=("E1003",),
        title="ESP32 PhotoFrame for E1003",
    ),
    ExternalFirmware(
        id="PhotoFrame_reTerminal_E1004",
        repo="aitjcize/esp32-photoframe",
        asset="photoframe-firmware-seeedstudio_reterminal_e1004-merged.bin",
        devices=("E1004",),
        title="ESP32 PhotoFrame for E1004",
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
                    if target.tool == "arduino" and target.chip == "esp32"
                ]
            },
            "nrf52_matrix": {
                "include": [
                    target.to_matrix()
                    for target in self.changed_targets
                    if target.tool == "arduino" and target.chip == "nrf52"
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
        if any(
            is_under(changed_file, target.path)
            or any(is_under(changed_file, trigger) for trigger in target.rebuild_triggers)
            for changed_file in changed_files
        )
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
        and FIRMWARE_VERSION_RE.match(entry.name)
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


def missing_artifact_parts(
    source_dir: Path,
    firmware_id: str,
    require_spiffs: bool = False,
) -> list[str]:
    """Return required firmware artifact patterns that are missing.

    返回缺失的必要固件产物模式。
    """

    missing: list[str] = []
    for pattern in ("*.ino.bootloader.bin", "*.ino.partitions.bin", "*.ino.bin"):
        if not sorted(source_dir.glob(pattern)):
            missing.append(pattern)
    if not (source_dir / "boot_app0.bin").exists():
        missing.append("boot_app0.bin")
    if require_spiffs and not (source_dir / f"{firmware_id}.spiffs.bin").exists():
        missing.append(f"{firmware_id}.spiffs.bin")
    return missing


def write_manifest(
    firmware_id: str,
    version: str,
    destination: Path,
    spiffs_offset: int = 0,
    boot_app0_offset: int = 0xE000,
    app_offset: int = 0x10000,
    flash_size: str = "keep",
    include_filesystem: bool = True,
) -> None:
    def part(file_name: str, offset: int) -> dict[str, Any]:
        return {
            "path": cache_busted_path(destination / file_name, file_name),
            "offset": offset,
        }

    parts = [
        part(f"{firmware_id}.ino.bootloader.bin", 0),
        part(f"{firmware_id}.ino.partitions.bin", 32768),
        part("boot_app0.bin", boot_app0_offset),
        part(f"{firmware_id}.ino.bin", app_offset),
    ]
    spiffs = destination / f"{firmware_id}.spiffs.bin"
    if include_filesystem and spiffs.exists():
        if not spiffs_offset:
            raise ValueError(f"Missing SPIFFS offset for {firmware_id}")
        parts.append(part(spiffs.name, spiffs_offset))

    manifest = {
        "name": firmware_id,
        "version": version,
        "flashSize": flash_size,
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


def copy_uf2_artifact(source_dir: Path, firmware_id: str, destination: Path) -> None:
    """Copy the built .uf2 into the published firmware directory.

    将构建出的 .uf2 复制到发布目录。
    """

    destination.mkdir(parents=True, exist_ok=True)
    matches = sorted(source_dir.glob("*.uf2"))
    if not matches:
        raise FileNotFoundError(f"Missing *.uf2 for {firmware_id}")
    shutil.copy2(matches[0], destination / f"{firmware_id}.uf2")


def write_uf2_manifest(firmware_id: str, version: str, destination: Path) -> None:
    """Emit version metadata for a UF2 firmware directory.

    为 UF2 固件目录生成版本元数据。
    """

    uf2 = destination / f"{firmware_id}.uf2"
    manifest = {
        "name": firmware_id,
        "version": version,
        "flashMethod": "uf2",
        "builds": [
            {
                "chipFamily": "nRF52840",
                "parts": [
                    {
                        "path": cache_busted_path(uf2, uf2.name),
                        "offset": 0,
                    }
                ],
            }
        ],
    }
    (destination / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n",
        encoding="utf-8",
    )


def download_binary(url: str, destination: Path) -> None:
    """Download a firmware binary to a local path.

    将固件二进制下载到本地路径。
    """

    destination.parent.mkdir(parents=True, exist_ok=True)
    request = urllib.request.Request(url, headers={"User-Agent": "firmware-release"})
    with urllib.request.urlopen(request) as response, destination.open("wb") as output:
        shutil.copyfileobj(response, output)


def latest_release_tag(repo: str) -> str:
    """Return the latest (non-prerelease) GitHub release tag for a repo, e.g.
    'v2.9.0'. Uses GITHUB_TOKEN/GH_TOKEN when present to avoid rate limits.

    返回仓库的最新（非预发布）GitHub Release 标签。
    """

    api_url = f"https://api.github.com/repos/{repo}/releases/latest"
    headers = {
        "User-Agent": "firmware-release",
        "Accept": "application/vnd.github+json",
    }
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(api_url, headers=headers)
    with urllib.request.urlopen(request) as response:
        data = json.load(response)
    tag = data.get("tag_name")
    if not tag:
        raise RuntimeError(f"No latest release tag found for {repo}")
    return tag


def write_external_manifest(
    external: ExternalFirmware, version: str, url: str, destination: Path
) -> None:
    """Mirror an external merged image and emit a same-origin manifest.

    镜像外部合并镜像并生成同源 manifest。
    """

    destination.mkdir(parents=True, exist_ok=True)
    bin_name = url.rsplit("/", 1)[-1]
    download_binary(url, destination / bin_name)

    manifest = {
        "name": external.title or external.id,
        "version": version,
        "new_install_prompt_erase": True,
        "builds": [
            {
                "chipFamily": external.chip_family,
                "parts": [
                    {
                        "path": cache_busted_path(destination / bin_name, bin_name),
                        "offset": 0,
                    }
                ],
            }
        ],
    }
    (destination / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n",
        encoding="utf-8",
    )


def publish_external_firmware(
    firmware_root: Path,
    external_firmware: tuple[ExternalFirmware, ...],
    resolve_tag: Callable[[str], str] = latest_release_tag,
) -> dict[str, str]:
    """Publish external firmware manifests at each repo's latest release version,
    skipping versions already mirrored. The tag is resolved once per repo.

    以各仓库最新 Release 版本发布外部固件 manifest，跳过已镜像的版本。
    """

    published: dict[str, str] = {}
    tags: dict[str, str] = {}
    for external in external_firmware:
        tag = tags.get(external.repo)
        if tag is None:
            tag = resolve_tag(external.repo)
            tags[external.repo] = tag
        version = tag.lstrip("v")
        destination = firmware_root / external.id / version
        if (destination / "manifest.json").exists():
            continue
        write_external_manifest(external, version, external.url_for(tag), destination)
        published[external.id] = version
        print(f"Published external firmware: {external.id} -> {version}")
    return published


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


def write_catalog_json(
    firmware_root: Path,
    targets: list[FirmwareTarget],
    external_firmware: tuple[ExternalFirmware, ...] = (),
) -> None:
    catalog_items = [target.to_catalog() for target in targets]
    catalog_items.extend(external.to_catalog() for external in external_firmware)
    known_ids = {target.id for target in targets}
    known_ids.update(external.id for external in external_firmware)
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


def copy_release_assets(
    firmware_root: Path,
    versions: dict[str, list[str]],
    release_dir: Path,
    exclude_ids: set[str] = frozenset(),
) -> None:
    release_dir.mkdir(parents=True, exist_ok=True)
    for firmware_id, firmware_versions in sorted(versions.items()):
        if not firmware_versions or firmware_id in exclude_ids:
            continue
        current_version = firmware_versions[0]
        source_dir = firmware_root / firmware_id / current_version
        artifacts = sorted(source_dir.glob("*.bin")) + sorted(source_dir.glob("*.uf2"))
        for artifact in artifacts:
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
        if target.chip == "nrf52":
            if not sorted(artifact_dir.glob("*.uf2")):
                skipped_targets[target.id] = "missing *.uf2"
                continue
            firmware_dir = firmware_root / target.id
            version = target.fixed_version or next_date_version(date_versions(firmware_dir), today)
            destination = firmware_dir / version
            copy_uf2_artifact(artifact_dir, target.id, destination)
            write_uf2_manifest(target.id, version, destination)
            changed_versions[target.id] = version
            print(f"Updated firmware: {target.id} -> {version}")
            continue
        missing_parts = missing_artifact_parts(
            artifact_dir,
            target.id,
            target.include_filesystem and bool(target.spiffs_offset),
        )
        if missing_parts:
            skipped_targets[target.id] = f"missing {', '.join(missing_parts)}"
            continue
        firmware_dir = firmware_root / target.id
        version = target.fixed_version or next_date_version(date_versions(firmware_dir), today)
        destination = firmware_dir / version
        copy_firmware_artifact(artifact_dir, target.id, destination)
        write_manifest(
            target.id,
            version,
            destination,
            target.spiffs_offset,
            target.boot_app0_offset,
            target.app_offset,
            target.flash_size,
            target.include_filesystem,
        )
        changed_versions[target.id] = version
        print(f"Updated firmware: {target.id} -> {version}")

    for firmware_id, reason in sorted(skipped_targets.items()):
        print(f"Skipped firmware: {firmware_id} ({reason})")

    publish_external_firmware(firmware_root, EXTERNAL_FIRMWARE)

    versions = write_versions_json(firmware_root)
    write_catalog_json(firmware_root, plan.all_targets, EXTERNAL_FIRMWARE)
    copy_release_assets(
        firmware_root,
        versions,
        release_assets_dir,
        exclude_ids={external.id for external in EXTERNAL_FIRMWARE},
    )
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
    nrf52_matrix = payload["nrf52_matrix"]
    pio_matrix = payload["pio_matrix"]
    write_github_outputs(
        args.github_output,
        {
            "firmware_changed": "true" if payload["firmware_changed"] else "false",
            "has_arduino": "true" if arduino_matrix["include"] else "false",
            "has_nrf52": "true" if nrf52_matrix["include"] else "false",
            "has_pio": "true" if pio_matrix["include"] else "false",
            "arduino_matrix": json.dumps(arduino_matrix, separators=(",", ":")),
            "nrf52_matrix": json.dumps(nrf52_matrix, separators=(",", ":")),
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

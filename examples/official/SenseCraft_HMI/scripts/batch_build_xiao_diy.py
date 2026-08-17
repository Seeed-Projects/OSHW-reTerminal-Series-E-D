#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parents[1]
DRIVER_H = ROOT / "src" / "driver.h"
CONFIG_H = ROOT / "src" / "app_config.h"
PIO_BUILD_DIR = ROOT / ".pio" / "build" / "xiao_diy_kit"
DEFAULT_OUTPUT_BASE_DIR = ROOT / "build" / "xiao_diy_kit_batch"

# Keep these bytes aligned with src/app_config.h. They are embedded in every
# APP binary so the release platform can identify its cloud environment.
FIRMWARE_ENV_MARKERS = {
    "test": "@SC_HMI_FW_ENV=TEST@",
    "production": "@SC_HMI_FW_ENV=PROD@",
}

SELECTION_BEGIN = "// XIAO DIY selection begin"
SELECTION_END = "// XIAO DIY selection end"


@dataclass(frozen=True)
class Target:
    board: str
    use_define: str
    combo: int
    slug: str
    label: str

    @property
    def combo_dir_name(self) -> str:
        return f"{self.combo}_{self.slug}"

    @property
    def selection_key(self) -> str:
        return f"{self.board}:{self.combo}"

    @property
    def firmware_filename(self) -> str:
        return f"{self.combo_dir_name}.bin"


@dataclass(frozen=True)
class BuildConfig:
    app_version: str
    api_env: str

    @property
    def test_env_macro(self) -> str:
        return "1" if self.api_env == "test" else "0"

    @property
    def firmware_env_marker(self) -> str:
        return FIRMWARE_ENV_MARKERS[self.api_env]


TARGETS: tuple[Target, ...] = (
    Target("ee02", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE02", 510, "13_3_color", "13.3 inch six-color"),
    Target("ee03", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE03", 511, "10_3_mono", "10.3 inch monochrome"),
    Target("ee04", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE04", 505, "1_54_mono", "1.54 inch monochrome"),
    Target("ee04", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE04", 517, "1_54_bwry", "1.54 inch BWRY"),
    Target("ee04", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE04", 508, "2_13_mono", "2.13 inch monochrome"),
    Target("ee04", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE04", 513, "2_13_bwry", "2.13 inch BWRY"),
    Target("ee04", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE04", 504, "2_9_mono", "2.9 inch monochrome"),
    Target("ee04", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE04", 512, "2_9_bwry", "2.9 inch BWRY"),
    Target("ee04", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE04", 506, "4_26_mono", "4.26 inch monochrome"),
    Target("ee04", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE04", 515, "3_97_mono", "3.97 inch monochrome"),
    Target("ee04", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE04", 516, "3_97_bwry", "3.97 inch BWRY"),
    Target("ee04", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE04", 509, "7_3_color", "7.3 inch six-color"),
    Target("ee04", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE04", 502, "7_5_mono", "7.5 inch monochrome"),
    Target("ee05", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE05", 505, "1_54_mono", "1.54 inch monochrome"),
    Target("ee05", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE05", 517, "1_54_bwry", "1.54 inch BWRY"),
    Target("ee05", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE05", 508, "2_13_mono", "2.13 inch monochrome"),
    Target("ee05", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE05", 513, "2_13_bwry", "2.13 inch BWRY"),
    Target("ee05", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE05", 504, "2_9_mono", "2.9 inch monochrome"),
    Target("ee05", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE05", 512, "2_9_bwry", "2.9 inch BWRY"),
    Target("ee05", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE05", 506, "4_26_mono", "4.26 inch monochrome"),
    Target("ee05", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE05", 515, "3_97_mono", "3.97 inch monochrome"),
    Target("ee05", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE05", 516, "3_97_bwry", "3.97 inch BWRY"),
    Target("ee05", "USE_XIAO_EPAPER_DISPLAY_BOARD_EE05", 502, "7_5_mono", "7.5 inch monochrome"),
)

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Batch-build XIAO DIY firmware with the xiao_diy_kit environment by "
            "commenting/uncommenting selections in src/driver.h."
        )
    )
    parser.add_argument(
        "targets",
        nargs="*",
        help=(
            "Target selectors. Use ee02 / ee03 / ee04 / ee05 for full board batches, "
            "or ee04:512 style to build a single combo."
        ),
    )
    parser.add_argument(
        "--output-dir",
        help="Base directory used to store copied build artifacts. Default: build/xiao_diy_kit_batch/<test|production>",
    )
    parser.add_argument(
        "--version",
        "--app-version",
        dest="app_version",
        help="Temporarily override g_currentAppVersion in src/app_config.h for this build run.",
    )
    parser.add_argument(
        "--env",
        "--api-env",
        dest="api_env",
        choices=("test", "production"),
        help="Temporarily override HMI_TEST_ENV in src/app_config.h for this build run.",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="Print the available batch targets and exit.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show the resolved build queue without calling PlatformIO.",
    )
    parser.add_argument(
        "--keep-last",
        action="store_true",
        help="Keep src/driver.h and src/app_config.h on the final build selection instead of restoring them.",
    )
    parser.add_argument(
        "--no-clean",
        action="store_true",
        help="Skip 'pio run -t clean' before each build.",
    )
    return parser.parse_args()


def list_targets() -> None:
    for board in ("ee02", "ee03", "ee04", "ee05"):
        board_targets = [target for target in TARGETS if target.board == board]
        combos = ", ".join(f"{target.combo}:{target.slug}" for target in board_targets)
        print(f"{board}: {combos}")


def resolve_targets(specs: Iterable[str]) -> list[Target]:
    if not specs:
        return list(TARGETS)

    resolved: list[Target] = []
    seen: set[str] = set()
    for spec in specs:
        board, sep, combo_text = spec.lower().partition(":")
        matches = [target for target in TARGETS if target.board == board]
        if not matches:
            raise SystemExit(f"Unknown board selector: {spec}")

        selected = matches
        if sep:
            try:
                combo = int(combo_text)
            except ValueError as exc:
                raise SystemExit(f"Invalid combo selector: {spec}") from exc
            selected = [target for target in matches if target.combo == combo]
            if not selected:
                raise SystemExit(f"Board {board} does not support combo {combo}")

        for target in selected:
            if target.selection_key not in seen:
                resolved.append(target)
                seen.add(target.selection_key)

    return resolved


def extract_selection_block(text: str) -> tuple[int, int]:
    begin = text.find(SELECTION_BEGIN)
    end = text.find(SELECTION_END)
    if begin == -1 or end == -1 or end <= begin:
        raise RuntimeError("Failed to locate the XIAO DIY selection block in src/driver.h")
    return begin, end


def set_driver_selection(original_text: str, target: Target) -> str:
    begin, end = extract_selection_block(original_text)
    block = original_text[begin:end]

    def toggle_use_define(match: re.Match[str]) -> str:
        indent = match.group("indent")
        define = match.group("define")
        suffix = match.group("suffix")
        prefix = "" if define == target.use_define else "// "
        return f"{indent}{prefix}#define {define}{suffix}"

    def toggle_combo(match: re.Match[str]) -> str:
        indent = match.group("indent")
        combo = int(match.group("combo"))
        suffix = match.group("suffix")
        prefix = "" if combo == target.combo else "// "
        return f"{indent}{prefix}#define BOARD_SCREEN_COMBO {combo}{suffix}"

    use_pattern = re.compile(
        r"^(?P<indent>\s*)(?://\s*)?#define (?P<define>USE_XIAO_EPAPER_DISPLAY_BOARD_EE0[2345])(?P<suffix>.*)$",
        re.MULTILINE,
    )
    combo_pattern = re.compile(
        r"^(?P<indent>\s*)(?://\s*)?#define BOARD_SCREEN_COMBO (?P<combo>\d+)(?P<suffix>.*)$",
        re.MULTILINE,
    )

    block = use_pattern.sub(toggle_use_define, block)
    block = combo_pattern.sub(toggle_combo, block)
    return f"{original_text[:begin]}{block}{original_text[end:]}"


def ensure_single_selection(text: str, target: Target) -> None:
    begin, end = extract_selection_block(text)
    block = text[begin:end]

    active_use = re.findall(r"^\s*#define (USE_XIAO_EPAPER_DISPLAY_BOARD_EE0[2345])\b", block, re.MULTILINE)
    active_combo = re.findall(r"^\s*#define BOARD_SCREEN_COMBO (\d+)\b", block, re.MULTILINE)

    if active_use != [target.use_define]:
        raise RuntimeError(f"Unexpected USE define selection after update: {active_use}")
    if active_combo != [str(target.combo)]:
        raise RuntimeError(f"Unexpected combo selection after update: {active_combo}")


def parse_build_config(text: str) -> BuildConfig:
    version_match = re.search(
        r'^\s*static const char\*\s*g_currentAppVersion\s*=\s*"(?P<version>[^"]+)";\s*$',
        text,
        re.MULTILINE,
    )
    env_match = re.search(
        r"^\s*#define HMI_TEST_ENV (?P<value>[01])\s*$",
        text,
        re.MULTILINE,
    )

    if version_match is None:
        raise RuntimeError("Failed to locate g_currentAppVersion in src/app_config.h")
    if env_match is None:
        raise RuntimeError("Failed to locate HMI_TEST_ENV in src/app_config.h")

    return BuildConfig(
        app_version=version_match.group("version"),
        api_env="test" if env_match.group("value") == "1" else "production",
    )


def set_build_config(original_text: str, build_config: BuildConfig) -> str:
    text = re.sub(
        r'^(?P<indent>\s*)static const char\*\s*g_currentAppVersion\s*=\s*"[^"]+";\s*$',
        f'\\g<indent>static const char* g_currentAppVersion = "{build_config.app_version}";',
        original_text,
        count=1,
        flags=re.MULTILINE,
    )
    text = re.sub(
        r"^(?P<indent>\s*)#define HMI_TEST_ENV [01]\s*$",
        f"\\g<indent>#define HMI_TEST_ENV {build_config.test_env_macro}",
        text,
        count=1,
        flags=re.MULTILINE,
    )
    return text


def run_command(args: list[str]) -> None:
    print("+", " ".join(args))
    subprocess.run(args, cwd=ROOT, check=True)


def resolve_output_dir(output_dir_arg: str | None, build_config: BuildConfig) -> Path:
    if output_dir_arg:
        return Path(output_dir_arg).resolve()
    return (DEFAULT_OUTPUT_BASE_DIR / build_config.api_env).resolve()


def validate_firmware_environment(firmware: Path, build_config: BuildConfig) -> None:
    data = firmware.read_bytes()
    expected_marker = build_config.firmware_env_marker
    other_env = "production" if build_config.api_env == "test" else "test"
    unexpected_marker = FIRMWARE_ENV_MARKERS[other_env]
    problems = []
    if expected_marker.encode("utf-8") not in data:
        problems.append(f"missing marker: {expected_marker}")
    if unexpected_marker.encode("utf-8") in data:
        problems.append(f"unexpected marker: {unexpected_marker}")
    if problems:
        raise RuntimeError(
            f"Firmware environment marker validation failed: {firmware}\n- "
            + "\n- ".join(problems)
        )
    print(f"Firmware environment marker verified: {expected_marker}")


def copy_artifacts(target: Target, output_dir: Path, build_config: BuildConfig) -> dict[str, str]:
    destination = output_dir / target.board / build_config.app_version
    destination.mkdir(parents=True, exist_ok=True)

    source = PIO_BUILD_DIR / "firmware.bin"
    if not source.exists():
        raise RuntimeError(f"Missing build artifact: {source}")

    dest = destination / target.firmware_filename
    shutil.copy2(source, dest)
    validate_firmware_environment(dest, build_config)

    try:
        artifact_path = str(dest.relative_to(ROOT))
    except ValueError:
        artifact_path = str(dest)

    return {
        "filename": target.firmware_filename,
        "path": artifact_path,
    }


def write_manifest(output_dir: Path, results: list[dict[str, object]]) -> None:
    manifest_path = output_dir / "manifest.json"
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(results, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    if args.list:
        list_targets()
        return 0

    queue = resolve_targets(args.targets)
    original_config = CONFIG_H.read_text(encoding="utf-8")
    current_build_config = parse_build_config(original_config)
    build_config = BuildConfig(
        app_version=args.app_version or current_build_config.app_version,
        api_env=args.api_env or current_build_config.api_env,
    )
    output_dir = resolve_output_dir(args.output_dir, build_config)

    if args.dry_run:
        print(
            f"config -> version={build_config.app_version}, api_env={build_config.api_env}, output_dir={output_dir}"
        )
        for target in queue:
            print(
                f"{target.board}:{target.combo} -> {target.use_define} ({target.label}) "
                f"-> {target.board}/{build_config.app_version}/{target.firmware_filename}"
            )
        return 0

    original_driver = DRIVER_H.read_text(encoding="utf-8")
    results: list[dict[str, object]] = []

    try:
        CONFIG_H.write_text(set_build_config(original_config, build_config), encoding="utf-8")

        for index, target in enumerate(queue, start=1):
            print(
                f"[{index}/{len(queue)}] Building {target.board}:{target.combo} {target.label} "
                f"(version={build_config.app_version}, api_env={build_config.api_env})"
            )
            updated_driver = set_driver_selection(original_driver, target)
            ensure_single_selection(updated_driver, target)
            DRIVER_H.write_text(updated_driver, encoding="utf-8")

            if not args.no_clean:
                run_command(["pio", "run", "-e", "xiao_diy_kit", "-t", "clean"])
            run_command(["pio", "run", "-e", "xiao_diy_kit"])

            copied = copy_artifacts(target, output_dir, build_config)
            results.append(
                {
                    "board": target.board,
                    "combo": target.combo,
                    "artifact_name": target.firmware_filename,
                    "use_define": target.use_define,
                    "label": target.label,
                    "app_version": build_config.app_version,
                    "api_env": build_config.api_env,
                    "firmware_env_marker": build_config.firmware_env_marker,
                    "firmware_bin": copied,
                }
            )
    finally:
        if args.keep_last and queue:
            DRIVER_H.write_text(set_driver_selection(original_driver, queue[-1]), encoding="utf-8")
            CONFIG_H.write_text(set_build_config(original_config, build_config), encoding="utf-8")
        else:
            DRIVER_H.write_text(original_driver, encoding="utf-8")
            CONFIG_H.write_text(original_config, encoding="utf-8")

    write_manifest(output_dir, results)
    print(f"Artifacts written to: {output_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

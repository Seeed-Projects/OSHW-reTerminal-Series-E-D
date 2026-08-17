#!/usr/bin/env python3
"""
Lightweight terminal configuration interface similar to menuconfig.
Run with:
    python3 config/menuconfig.py
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

from cursesmenu import CursesMenu
from cursesmenu.items import FunctionItem

ROOT_DIR = Path(__file__).resolve().parent
CONFIG_FILE = ROOT_DIR / "settings.json"

DEFAULT_CONFIG = {
    "debug": False,
    "environment": "release",
    "display": "epaper",
}

ENV_OPTIONS = ["release", "staging", "test"]
DISPLAY_OPTIONS = ["epaper", "lcd"]


def load_config() -> dict:
    if not CONFIG_FILE.exists():
        save_config(DEFAULT_CONFIG)
        return dict(DEFAULT_CONFIG)
    with CONFIG_FILE.open("r", encoding="utf-8") as fp:
        try:
            data = json.load(fp)
        except json.JSONDecodeError:
            data = dict(DEFAULT_CONFIG)
    # Fill in any missing keys with default values
    for key, value in DEFAULT_CONFIG.items():
        data.setdefault(key, value)
    return data


def save_config(config: dict) -> None:
    CONFIG_FILE.parent.mkdir(parents=True, exist_ok=True)
    with CONFIG_FILE.open("w", encoding="utf-8") as fp:
        json.dump(config, fp, indent=2)


class ConfigMenu:
    """Wraps the curses menu to display and update configuration."""

    def __init__(self) -> None:
        self.config = load_config()
        self.menu = CursesMenu("Project Configuration", "Use arrow keys to navigate, Enter to confirm, Q to exit")

        self.debug_item = FunctionItem(
            self._fmt_debug_text(),
            self.toggle_debug,
        )
        self.env_item = FunctionItem(
            self._fmt_env_text(),
            self.choose_environment,
        )
        self.display_item = FunctionItem(
            self._fmt_display_text(),
            self.choose_display,
        )
        self.defaults_item = FunctionItem(
            "Restore Defaults",
            self.reset_defaults,
        )
        self.menu.items.append(self.debug_item)
        self.menu.items.append(self.env_item)
        self.menu.items.append(self.display_item)
        self.menu.items.append(self.defaults_item)

    def _fmt_debug_text(self) -> str:
        return f"Debug Mode: {'On' if self.config.get('debug') else 'Off'}"

    def _fmt_env_text(self) -> str:
        env = self.config.get("environment")
        return f"Environment: {env}"

    def _fmt_display_text(self) -> str:
        display = self.config.get("display")
        return f"Display Driver: {display}"

    def toggle_debug(self) -> None:
        self.config["debug"] = not self.config.get("debug", False)
        save_config(self.config)
        self.debug_item.text = self._fmt_debug_text()

    def choose_environment(self) -> None:
        options = [
            f"{value}{' (Current)' if value == self.config.get('environment') else ''}"
            for value in ENV_OPTIONS
        ]
        submenu = CursesMenu.make_selection_menu(
            options,
            title="Select Environment",
            subtitle="Press Enter to select, Q to return",
            show_exit_item=True,
        )
        submenu.parent = self.menu
        submenu.show()
        selected = submenu.join()
        if selected is not None and 0 <= selected < len(ENV_OPTIONS):
            self.config["environment"] = ENV_OPTIONS[selected]
            save_config(self.config)
            self.env_item.text = self._fmt_env_text()

    def choose_display(self) -> None:
        options = [
            f"{value}{' (Current)' if value == self.config.get('display') else ''}"
            for value in DISPLAY_OPTIONS
        ]
        submenu = CursesMenu.make_selection_menu(
            options,
            title="Select Display",
            subtitle="Press Enter to select, Q to return",
            show_exit_item=True,
        )
        submenu.parent = self.menu
        submenu.show()
        selected = submenu.join()
        if selected is not None and 0 <= selected < len(DISPLAY_OPTIONS):
            self.config["display"] = DISPLAY_OPTIONS[selected]
            save_config(self.config)
            self.display_item.text = self._fmt_display_text()

    def reset_defaults(self) -> None:
        self.config = dict(DEFAULT_CONFIG)
        save_config(self.config)
        self.debug_item.text = self._fmt_debug_text()
        self.env_item.text = self._fmt_env_text()
        self.display_item.text = self._fmt_display_text()

    def run(self) -> None:
        self.menu.show()
        self.menu.join()


def main() -> None:
    if not (sys.stdin.isatty() and sys.stdout.isatty()):
        print("Please run this script inside an interactive terminal, e.g., python3 config/menuconfig.py")
        return
    ConfigMenu().run()


if __name__ == "__main__":
    main()

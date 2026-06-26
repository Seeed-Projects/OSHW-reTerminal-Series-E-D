"""
PlatformIO pre-build script.
Auto-patches EEZ Studio exported UI files for ePaper compatibility:
  1. lv_scr_load_anim(...) -> lv_scr_load(scr)
  2. #include <lvgl/lvgl.h> -> #include <lvgl.h>
"""

import os
import re
import glob

Import("env")

UI_DIR = os.path.join(env.subst("$PROJECT_DIR"), "src", "ui")

PATCHES = [
    # lv_scr_load_anim(screen, ANIM, time, delay, bool) -> lv_scr_load(screen)
    (
        re.compile(
            r'lv_scr_load_anim\s*\(\s*([^,]+?)\s*,'  # first arg (screen)
            r'[^)]*\)'                                 # remaining args
        ),
        r'lv_scr_load(\1)',
    ),
    # #include <lvgl/lvgl.h> -> #include <lvgl.h>
    (
        re.compile(r'#\s*include\s*<lvgl/lvgl\.h>'),
        r'#include <lvgl.h>',
    ),
]

if os.path.isdir(UI_DIR):
    for path in glob.glob(os.path.join(UI_DIR, "*.[ch]")):
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            original = f.read()

        patched = original
        for pattern, replacement in PATCHES:
            patched = pattern.sub(replacement, patched)

        if patched != original:
            with open(path, "w", encoding="utf-8") as f:
                f.write(patched)
            print(f"[pre_build] patched: {os.path.basename(path)}")

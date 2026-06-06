# Pre-build hook: embed the Chinese TTF into the firmware as a C byte array, so
# the font ships inside the binary and needs no SPIFFS / mkspiffs / uploadfs.
# Regenerates src/FontZH.h from data/test_ZH.ttf only when missing or stale.
# Runs both as a PlatformIO pre-script and as a standalone `python3` script.
import os

SRC = os.path.join("data", "test_ZH.ttf")
OUT = os.path.join("src", "FontZH.h")


def up_to_date():
    return (os.path.exists(OUT)
            and os.path.getmtime(OUT) >= os.path.getmtime(SRC))


def generate():
    with open(SRC, "rb") as f:
        data = f.read()

    parts = [
        "#pragma once",
        "// Auto-generated from %s by scripts/gen_font.py." % SRC,
        "// Do not edit by hand; it is regenerated on the Chinese build.",
        "#include <cstddef>",
        "",
        "const unsigned char vm_font_zh[] = {",
    ]
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        parts.append("  " + "".join("0x%02x," % b for b in chunk))
    parts.append("};")
    parts.append("const size_t vm_font_zh_len = %d;" % len(data))
    parts.append("")

    with open(OUT, "w") as f:
        f.write("\n".join(parts))
    print("[gen_font] wrote %s (%d bytes)" % (OUT, len(data)))


if not os.path.exists(SRC):
    print("[gen_font] WARNING: %s not found; skipping" % SRC)
elif up_to_date():
    print("[gen_font] %s is up to date" % OUT)
else:
    generate()

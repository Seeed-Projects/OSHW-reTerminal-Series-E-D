#!/usr/bin/env python3
"""Batch-prepare photos for the reTerminal E10xx SD slideshow examples.

The sketches decode to RGB888 in PSRAM and then dither with an error buffer,
so sources must be small enough. With the E1003 default DITHER_FS budget
(1.32 MB sprite + 6 bytes/pixel peak) stay at or below about 1100 x 825.
The E1004 six-color panel uses a 0.92 MB 4-bit sprite and a ~4 bytes/pixel
budget; use --mode cover --max-width 1110 --max-height 1482.

Usage:
    python prepare_photos.py <input-dir-or-file> <output-dir>
    python prepare_photos.py Photos out --max-width 1450 --max-height 1080
    python prepare_photos.py Photos out --mode contain  # exact canvas

Defaults: fit inside 1100x825, no upscale, 4:3-friendly, baseline JPEG q92.
Output files are numbered 001_..., 002_... so the on-device alphabetical
sort matches the order you see here. Copy the whole output folder to the SD
card as /img.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image, ImageOps
except ImportError:
    sys.exit("Pillow is required:  python -m pip install pillow")

SUPPORTED = {".jpg", ".jpeg", ".png", ".bmp", ".webp", ".tif", ".tiff"}


def load_rgb(path: Path) -> Image.Image:
    im = Image.open(path)
    im = ImageOps.exif_transpose(im)  # honor phone rotation metadata

    if im.mode in ("RGBA", "LA") or (im.mode == "P" and "transparency" in im.info):
        im = im.convert("RGBA")
        bg = Image.new("RGB", im.size, (255, 255, 255))
        bg.paste(im, mask=im.getchannel("A"))
        return bg

    return im.convert("RGB")


def even(value: int) -> int:
    return max(2, value - (value & 1))


def prepare_one(src: Path, dst: Path, max_w: int, max_h: int,
                quality: int, mode: str) -> tuple[int, int]:
    im = load_rgb(src)

    if mode == "contain":
        # Keep aspect ratio and pad to the exact max canvas with white, so the
        # photo fills the requested 4:3 area without cropping.
        canvas = Image.new("RGB", (max_w, max_h), (255, 255, 255))
        fitted = ImageOps.contain(im, (max_w, max_h), Image.LANCZOS)
        canvas.paste(fitted, ((max_w - fitted.width) // 2,
                              (max_h - fitted.height) // 2))
        out = canvas
    elif mode == "cover":
        # Crop to the canvas aspect ratio and fill it completely. Use this for
        # a frame whose panel matches max_w:max_h (e.g. 1110x1482 on the
        # portrait 1200x1600 E1004).
        out = ImageOps.fit(im, (max_w, max_h), Image.LANCZOS)
        out = out.resize((even(out.width), even(out.height)), Image.LANCZOS)
    else:
        out = im.copy()
        if out.width > max_w or out.height > max_h:
            out.thumbnail((max_w, max_h), Image.LANCZOS)
        out = out.resize((even(out.width), even(out.height)), Image.LANCZOS)

    out.save(dst, "JPEG", quality=quality, optimize=True, progressive=False)
    return out.width, out.height


def estimate_peak_mb(w: int, h: int, method: str, sprite_mb: float) -> float:
    n = w * h
    per_px = 4 if method.lower() == "bayer8" else 6
    return (sprite_mb * 1024 * 1024 + per_px * n) / (1024 * 1024)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", help="photo file or folder (recursed)")
    ap.add_argument("output", help="output folder (copy to SD as /img)")
    ap.add_argument("--max-width", type=int, default=1100)
    ap.add_argument("--max-height", type=int, default=825)
    ap.add_argument("--quality", type=int, default=92)
    ap.add_argument("--mode", choices=("fit", "contain", "cover"), default="fit",
                    help="fit = keep aspect (white margins); contain = pad to max canvas; "
                         "cover = crop to the canvas aspect and fill it")
    ap.add_argument("--dither", choices=("fs", "bayer8"), default="fs",
                    help="which dither the sketch will use (for the memory estimate)")
    ap.add_argument("--sprite-mb", type=float, default=1.32,
                    help="4bpp sprite size of the panel, for the memory estimate: "
                         "E1003 1872x1404 -> 1.32, E1004 1200x1600 -> 0.92")
    args = ap.parse_args()

    src = Path(args.input)
    files = [src] if src.is_file() else sorted(
        p for p in src.rglob("*") if p.is_file() and p.suffix.lower() in SUPPORTED)

    if not files:
        print(f"No supported images found in {src}")
        return 2

    out_dir = Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"max canvas : {args.max_width} x {args.max_height}  (mode={args.mode})")
    print(f"dither     : {args.dither}  (used only for the memory estimate)\n")

    made = 0
    worst = 0.0
    for i, path in enumerate(files, start=1):
        dst = out_dir / f"{i:03d}_{path.stem}.jpg"
        try:
            w, h = prepare_one(path, dst, args.max_width, args.max_height,
                               args.quality, args.mode)
        except Exception as exc:  # noqa: BLE001 - report and continue
            print(f"[skip] {path.name}: {exc}")
            continue
        peak = estimate_peak_mb(w, h, args.dither, args.sprite_mb)
        worst = max(worst, peak)
        flag = "  <-- LOW MARGIN" if peak > 7.4 else ""
        print(f"[ok] {path.name} -> {dst.name}  {w}x{h}  peak~{peak:.2f} MB{flag}")
        made += 1

    print(f"\n{made} file(s) written to {out_dir}")
    print(f"worst-case peak PSRAM: {worst:.2f} MB of 8 MB")
    if worst > 7.4:
        print("WARNING: some images are close to the PSRAM limit. "
              "Use a smaller canvas or switch the sketch to DITHER_BAYER8.")
    print("\nNext: copy the generated folder to the SD card as /img, "
          "insert the card, and reset the board.")
    return 0 if made else 1


if __name__ == "__main__":
    raise SystemExit(main())
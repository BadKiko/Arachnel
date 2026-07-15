#!/usr/bin/env python3
"""Export Arachnel app icons from the letter-A spider master PNG (no SVG required)."""

from __future__ import annotations

from collections import Counter
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
# Prefer curated master in assets/, else resources master.
CURSOR_ASSETS = Path(r"C:\Users\kiril\.cursor\projects\d-Work-Arachnel\assets")
SOURCES = [
    CURSOR_ASSETS / "arachnel-letter-a.png",
    ROOT / "resources" / "icons" / "arachnel-master.png",
]
PNG_DIR = ROOT / "resources" / "icons" / "png"
ICO_PATH = ROOT / "resources" / "icons" / "arachnel.ico"
MASTER_OUT = ROOT / "resources" / "icons" / "arachnel-master.png"
LINUX_DIR = ROOT / "resources" / "linux" / "icons" / "hicolor"

PNG_SIZES = [16, 22, 24, 32, 48, 64, 128, 256, 512]
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]


def harden(im: Image.Image) -> Image.Image:
    im = im.convert("RGBA")
    px = im.load()
    w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a < 40:
                px[x, y] = (0, 0, 0, 0)
                continue
            if (r + g + b) / 3.0 >= 140:
                px[x, y] = (255, 255, 255, 255)
            else:
                px[x, y] = (0, 0, 0, 255)
    return im


def rounded_black(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (0, 0, size - 1, size - 1), radius=int(size * 0.22), fill=255
    )
    black = Image.new("RGBA", (size, size), (0, 0, 0, 255))
    img.paste(black, (0, 0), mask)
    return img


def make(size: int, master: Image.Image) -> Image.Image:
    resample = Image.Resampling.LANCZOS if size >= 48 else Image.Resampling.BOX
    scaled = harden(master.resize((size, size), resample))
    badge = rounded_black(size)
    sp = scaled.load()
    bp = badge.load()
    for y in range(size):
        for x in range(size):
            r, g, b, a = sp[x, y]
            if a >= 32 and (r + g + b) / 3 >= 128 and bp[x, y][3] >= 200:
                bp[x, y] = (255, 255, 255, 255)
    return badge


def main() -> int:
    src = next((p for p in SOURCES if p.exists()), None)
    if src is None:
        raise SystemExit(f"no master found among {SOURCES}")

    master = harden(Image.open(src))
    PNG_DIR.mkdir(parents=True, exist_ok=True)
    master.save(MASTER_OUT, format="PNG", optimize=True)
    print(f"wrote {MASTER_OUT.relative_to(ROOT)} from {src.name}")

    for size in PNG_SIZES:
        out = make(size, master)
        path = PNG_DIR / f"{size}.png"
        out.save(path, format="PNG", optimize=True)
        top = Counter(out.getdata()).most_common(2)
        print(f"wrote {path.relative_to(ROOT)} {top}")

    ico_images = [Image.open(PNG_DIR / f"{s}.png").convert("RGBA") for s in ICO_SIZES]
    ico_images[0].save(
        ICO_PATH,
        format="ICO",
        sizes=[(im.width, im.height) for im in ico_images],
        append_images=ico_images[1:],
    )
    print(f"wrote {ICO_PATH.relative_to(ROOT)}")

    for size in PNG_SIZES:
        dest = LINUX_DIR / f"{size}x{size}" / "apps" / "arachnel.png"
        dest.parent.mkdir(parents=True, exist_ok=True)
        Image.open(PNG_DIR / f"{size}.png").save(dest, format="PNG", optimize=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

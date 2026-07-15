#!/usr/bin/env python3
"""Export Arachnel app icons from the letter-A spider master PNG.

GitHub SVGs (arachnel.svg / arachnel-github.svg) are separate — do not overwrite them.
"""

from __future__ import annotations

from collections import Counter
from pathlib import Path
import struct

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
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


def rgba_to_bmp_dib(im: Image.Image) -> bytes:
    """BITMAPINFOHEADER + BGRA XOR + empty AND mask (Win32 ICO)."""
    im = im.convert("RGBA")
    w, h = im.size
    pixels = list(im.getdata())
    xor = bytearray()
    for y in range(h - 1, -1, -1):
        for x in range(w):
            r, g, b, a = pixels[y * w + x]
            xor += bytes((b, g, r, a))
    row_bytes = ((w + 31) // 32) * 4
    and_mask = bytes(row_bytes * h)
    header = struct.pack(
        "<IiiHHIIiiII",
        40,
        w,
        h * 2,
        1,
        32,
        0,
        len(xor),
        0,
        0,
        0,
        0,
    )
    return header + bytes(xor) + and_mask


def write_win_ico(path: Path, images: list[Image.Image]) -> None:
    """Multi-size ICO via BMP DIBs — Pillow 12 only writes a single frame for ICO."""
    dibs = [rgba_to_bmp_dib(im) for im in images]
    count = len(images)
    offset = 6 + 16 * count
    entries: list[bytes] = []
    data = b""
    for im, dib in zip(images, dibs):
        w, h = im.size
        entries.append(
            struct.pack(
                "<BBBBHHII",
                0 if w >= 256 else w,
                0 if h >= 256 else h,
                0,
                0,
                1,
                32,
                len(dib),
                offset + len(data),
            )
        )
        data += dib
    path.write_bytes(struct.pack("<HHH", 0, 1, count) + b"".join(entries) + data)


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
        print(f"wrote {path.relative_to(ROOT)} {Counter(out.getdata()).most_common(2)}")

    ico_images = [Image.open(PNG_DIR / f"{s}.png").convert("RGBA") for s in ICO_SIZES]
    write_win_ico(ICO_PATH, ico_images)
    print(f"wrote {ICO_PATH.relative_to(ROOT)} ({ICO_PATH.stat().st_size} bytes)")

    for size in PNG_SIZES:
        dest = LINUX_DIR / f"{size}x{size}" / "apps" / "arachnel.png"
        dest.parent.mkdir(parents=True, exist_ok=True)
        Image.open(PNG_DIR / f"{size}.png").save(dest, format="PNG", optimize=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

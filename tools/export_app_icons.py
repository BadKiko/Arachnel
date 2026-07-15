#!/usr/bin/env python3
"""Export Arachnel icons from the master spider PNG into app resources."""

from __future__ import annotations

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
MASTER = Path(
    r"C:\Users\kiril\.cursor\projects\d-Work-Arachnel\assets\arachnel-icon-master-v2.png"
)
PNG_DIR = ROOT / "resources" / "icons" / "png"
ICO_PATH = ROOT / "resources" / "icons" / "arachnel.ico"
MASTER_OUT = ROOT / "resources" / "icons" / "arachnel-master.png"
LINUX_DIR = ROOT / "resources" / "linux" / "icons" / "hicolor"

PNG_SIZES = [16, 22, 24, 32, 48, 64, 128, 256, 512]
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]


def resize(img: Image.Image, size: int) -> Image.Image:
    return img.resize((size, size), Image.Resampling.LANCZOS)


def main() -> int:
    if not MASTER.exists():
        raise SystemExit(f"missing master: {MASTER}")

    master = Image.open(MASTER).convert("RGBA")
    PNG_DIR.mkdir(parents=True, exist_ok=True)
    master.save(MASTER_OUT, format="PNG", optimize=True)
    print(f"wrote {MASTER_OUT.relative_to(ROOT)}")

    for size in PNG_SIZES:
        out = PNG_DIR / f"{size}.png"
        resize(master, size).save(out, format="PNG", optimize=True)
        print(f"wrote {out.relative_to(ROOT)}")

    ico_images = [resize(master, size) for size in ICO_SIZES]
    ico_images[0].save(
        ICO_PATH,
        format="ICO",
        sizes=[(im.width, im.height) for im in ico_images],
        append_images=ico_images[1:],
    )
    print(f"wrote {ICO_PATH.relative_to(ROOT)}")

    for size in PNG_SIZES:
        dest_dir = LINUX_DIR / f"{size}x{size}" / "apps"
        dest_dir.mkdir(parents=True, exist_ok=True)
        dest = dest_dir / "arachnel.png"
        resize(master, size).save(dest, format="PNG", optimize=True)
        print(f"wrote {dest.relative_to(ROOT)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

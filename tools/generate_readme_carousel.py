#!/usr/bin/env python3
"""Generate animated README screenshot carousel SVG (self-contained for GitHub)."""

from __future__ import annotations

import base64
import io
import subprocess
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
IMAGES_DIR = ROOT / "images"
OUT_SVG = ROOT / "docs" / "readme-carousel.svg"

SRC_W = 1440
SRC_H = 900

CARD_W = 320
CARD_H = CARD_W * SRC_H // SRC_W
GAP = 16
RADIUS = 12
ROW_GAP = 14
MARGIN_Y = 12
VIEW_W = 920
VIEW_H = MARGIN_Y * 2 + CARD_H * 2 + ROW_GAP

EMBED_W = 640
EMBED_H = EMBED_W * SRC_H // SRC_W
CLIP_RX = RADIUS * EMBED_W // CARD_W


def slug(name: str) -> str:
    return Path(name).stem.replace(".", "-")


def encode_png(path: Path) -> str:
    image = Image.open(path).convert("RGB")
    image = image.resize((EMBED_W, EMBED_H), Image.Resampling.LANCZOS)
    buffer = io.BytesIO()
    image.save(buffer, format="PNG", optimize=True)
    return base64.b64encode(buffer.getvalue()).decode("ascii")


def build_defs(names: list[str]) -> str:
    image_defs: list[str] = []
    for name in names:
        ident = slug(name)
        data = encode_png(IMAGES_DIR / name)
        image_defs.append(
            f'    <image id="shot-{ident}" width="{EMBED_W}" height="{EMBED_H}" '
            f'preserveAspectRatio="xMidYMid meet" '
            f'href="data:image/png;base64,{data}"/>'
        )

    clip_def = (
        f'    <clipPath id="clip-shot" clipPathUnits="userSpaceOnUse">'
        f'<rect width="{EMBED_W}" height="{EMBED_H}" rx="{CLIP_RX}"/>'
        f"</clipPath>"
    )
    return "\n".join(image_defs + [clip_def])


def card_block(x: float, image_name: str) -> str:
    ident = slug(image_name)
    return f"""
      <g transform="translate({x} 0)">
        <svg width="{CARD_W}" height="{CARD_H}" viewBox="0 0 {EMBED_W} {EMBED_H}" overflow="hidden">
          <use href="#shot-{ident}" xlink:href="#shot-{ident}" width="{EMBED_W}" height="{EMBED_H}"
               clip-path="url(#clip-shot)"/>
        </svg>
        <rect width="{CARD_W}" height="{CARD_H}" rx="{RADIUS}" fill="none" stroke="#2a2a30" stroke-width="1"/>
      </g>"""


def row_strip(
    row_id: str,
    images: list[str],
    *,
    duration: float,
    start_offset: float,
) -> str:
    step = CARD_W + GAP
    set_width = len(images) * step
    end_offset = start_offset - set_width

    copies: list[str] = []
    for copy_index in range(2):
        cards = [card_block(index * step, name) for index, name in enumerate(images)]
        offset = 0 if copy_index == 0 else set_width
        copies.append(
            f'      <g transform="translate({offset} 0)">{"".join(cards)}\n      </g>'
        )

    row_y = MARGIN_Y if row_id == "top" else MARGIN_Y + CARD_H + ROW_GAP
    return f"""
    <g transform="translate(0 {row_y})">
      <g>
        <animateTransform attributeName="transform" attributeType="XML" type="translate"
          calcMode="linear" from="{start_offset} 0" to="{end_offset} 0"
          dur="{duration:.0f}s" repeatCount="indefinite"/>
{chr(10).join(copies)}
      </g>
    </g>"""


def main() -> None:
    names = [p.name for p in sorted(IMAGES_DIR.glob("*.png"))]
    if not names:
        raise SystemExit("No screenshots in images/")

    step = CARD_W + GAP
    stagger = step / 2

    defs = build_defs(names)
    top_row = row_strip("top", names, duration=52, start_offset=0)
    bottom_row = row_strip(
        "bottom",
        list(reversed(names)),
        duration=68,
        start_offset=-stagger,
    )

    svg = f"""<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink"
     viewBox="0 0 {VIEW_W} {VIEW_H}" role="img" aria-label="Arachnel screenshots">
  <defs>
{defs}
    <clipPath id="viewport">
      <rect width="{VIEW_W}" height="{VIEW_H}"/>
    </clipPath>
  </defs>
  <g clip-path="url(#viewport)">
    {top_row}
    {bottom_row}
  </g>
</svg>
"""
    OUT_SVG.parent.mkdir(parents=True, exist_ok=True)
    OUT_SVG.write_text(svg.strip() + "\n", encoding="utf-8")
    size_mb = OUT_SVG.stat().st_size / (1024 * 1024)
    print(f"Wrote {OUT_SVG} ({size_mb:.2f} MiB, cards {CARD_W}x{CARD_H} 16:10)")

    verify = Path(__file__).with_name("verify_readme_carousel.py")
    result = subprocess.run([sys.executable, str(verify)], check=False)
    if result.returncode != 0:
        raise SystemExit("Carousel verification failed")


if __name__ == "__main__":
    main()

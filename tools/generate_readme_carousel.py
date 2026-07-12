#!/usr/bin/env python3
"""Generate animated README screenshot carousel SVG."""

from __future__ import annotations

import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMAGES_DIR = ROOT / "images"
OUT_SVG = ROOT / "docs" / "readme-carousel.svg"

CARD_W = 360
GAP = 18
PAD = 8
RADIUS = 18
SCALE_W = CARD_W - PAD * 2
FULL_H = SCALE_W * 900 / 1440
HALF_H = FULL_H / 2
ROW_H = int(HALF_H + PAD * 2)
VIEW_W = 920
VIEW_H = ROW_H * 2 + 24
GITHUB_RAW = "https://raw.githubusercontent.com/BadKiko/Arachnel/develop/images"


def card_block(
    x: float,
    y: float,
    clip_id: str,
    image_name: str,
    *,
    top_half: bool,
) -> str:
    img_y = PAD - (0 if top_half else HALF_H)
    clip_y = PAD
    clip_h = HALF_H
    href = f"{GITHUB_RAW}/{image_name}"
    return f"""
    <g transform="translate({x:.1f} {y:.1f})">
      <rect width="{CARD_W}" height="{ROW_H}" rx="{RADIUS}" fill="#161618" stroke="#303036" stroke-width="1"/>
      <clipPath id="{clip_id}">
        <rect x="{PAD}" y="{clip_y}" width="{SCALE_W}" height="{clip_h}" rx="10"/>
      </clipPath>
      <image href="{href}" x="{PAD}" y="{img_y:.1f}" width="{SCALE_W}" height="{FULL_H:.1f}"
             clip-path="url(#{clip_id})" preserveAspectRatio="xMidYMid slice"/>
    </g>"""


def row_group(
    row_id: str,
    y: float,
    images: list[str],
    *,
    top_half: bool,
    duration: float,
    start_offset: float,
) -> str:
    step = CARD_W + GAP
    set_width = len(images) * step
    cards: list[str] = []
    for index, name in enumerate(images):
        clip_id = f"{row_id}-clip-{index}"
        cards.append(
            card_block(index * step, y, clip_id, name, top_half=top_half)
        )
    end_offset = start_offset - set_width
    return f"""
  <g id="{row_id}">
    <animateTransform attributeName="transform" attributeType="XML" type="translate"
      from="{start_offset:.1f} 0" to="{end_offset:.1f} 0" dur="{duration:.0f}s" repeatCount="indefinite"/>
    <g>
      {''.join(cards)}
    </g>
    <g transform="translate({set_width:.1f} 0)">
      {''.join(cards)}
    </g>
  </g>"""


def main() -> None:
    names = [p.name for p in sorted(IMAGES_DIR.glob("*.png"))]
    if not names:
        raise SystemExit("No screenshots in images/")

    top_row = row_group(
        "row-top",
        12,
        names,
        top_half=True,
        duration=48,
        start_offset=0,
    )
    bottom_row = row_group(
        "row-bottom",
        12 + ROW_H + 8,
        list(reversed(names)),
        top_half=False,
        duration=62,
        start_offset=-420,
    )

    svg = f"""<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {VIEW_W} {VIEW_H}" role="img" aria-label="Arachnel screenshots">
  <defs>
    <linearGradient id="fade-left" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#0d0d0f" stop-opacity="1"/>
      <stop offset="1" stop-color="#0d0d0f" stop-opacity="0"/>
    </linearGradient>
    <linearGradient id="fade-right" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#0d0d0f" stop-opacity="0"/>
      <stop offset="1" stop-color="#0d0d0f" stop-opacity="1"/>
    </linearGradient>
  </defs>
  <rect width="{VIEW_W}" height="{VIEW_H}" fill="#0d0d0f"/>
  <clipPath id="viewport">
    <rect x="0" y="0" width="{VIEW_W}" height="{VIEW_H}"/>
  </clipPath>
  <g clip-path="url(#viewport)">
    {top_row}
    {bottom_row}
  </g>
  <rect x="0" y="0" width="72" height="{VIEW_H}" fill="url(#fade-left)"/>
  <rect x="{VIEW_W - 72}" y="0" width="72" height="{VIEW_H}" fill="url(#fade-right)"/>
</svg>
"""
    OUT_SVG.parent.mkdir(parents=True, exist_ok=True)
    OUT_SVG.write_text(svg.strip() + "\n", encoding="utf-8")
    print(f"Wrote {OUT_SVG}")


if __name__ == "__main__":
    main()

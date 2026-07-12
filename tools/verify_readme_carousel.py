#!/usr/bin/env python3
"""Verify readme carousel cards show full 16:10 screenshots without crop."""

from __future__ import annotations

import base64
import io
import re
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
IMAGES_DIR = ROOT / "images"
SVG_PATH = ROOT / "docs" / "readme-carousel.svg"
PREVIEW_PATH = ROOT / "docs" / "readme-carousel-verify.png"
FRAME_PATH = ROOT / "docs" / "readme-carousel-frame.png"
SVG_RENDER_PATH = ROOT / "docs" / "readme-carousel-render.png"

CARD_W = 320
CARD_H = 200
COMPARE_GAP = 8
SRC_W = 1440
SRC_H = 900
VIEW_W = 920
VIEW_H = 438
ROW_GAP = 14
MARGIN_Y = 12
CAROUSEL_GAP = 16
MAX_RENDER_DIFF = 18.0


def load_embedded_images(svg_text: str) -> dict[str, Image.Image]:
    pattern = re.compile(
        r'<image id="shot-([^"]+)"[^>]*href="data:image/png;base64,([^"]+)"',
    )
    images: dict[str, Image.Image] = {}
    for ident, data in pattern.findall(svg_text):
        images[ident] = Image.open(io.BytesIO(base64.b64decode(data))).convert("RGB")
    return images


def expected_card(source: Image.Image) -> Image.Image:
    return source.resize((CARD_W, CARD_H), Image.Resampling.LANCZOS)


def card_from_embedded(embedded: Image.Image) -> Image.Image:
    return embedded.resize((CARD_W, CARD_H), Image.Resampling.LANCZOS)


def diff_score(a: Image.Image, b: Image.Image) -> float:
    if a.size != b.size:
        a = a.resize(b.size, Image.Resampling.LANCZOS)
    diff = ImageChops.difference(a.convert("RGB"), b.convert("RGB"))
    hist = diff.histogram()
    pixels = b.width * b.height
    total = 0
    for channel in range(3):
        channel_hist = hist[channel * 256 : (channel + 1) * 256]
        total += sum(value * index for index, value in enumerate(channel_hist))
    return total / (pixels * 3)


def build_frame(names: list[str], embedded: dict[str, Image.Image]) -> Image.Image:
    frame = Image.new("RGB", (VIEW_W, VIEW_H), "#0d0d0f")
    rows = (
        [Path(name).stem.replace(".", "-") for name in names],
        [Path(name).stem.replace(".", "-") for name in reversed(names)],
    )
    for row_index, row in enumerate(rows):
        y = MARGIN_Y + row_index * (CARD_H + ROW_GAP)
        x0 = 0 if row_index == 0 else -(CARD_W + CAROUSEL_GAP) // 2
        for copy in range(2):
            for card_index, ident in enumerate(row):
                x = x0 + copy * len(row) * (CARD_W + CAROUSEL_GAP) + card_index * (
                    CARD_W + CAROUSEL_GAP
                )
                if -CARD_W < x < frame.width:
                    card = embedded[ident].resize((CARD_W, CARD_H), Image.Resampling.LANCZOS)
                    frame.paste(card, (max(0, x), y))
    return frame


def render_svg() -> Image.Image | None:
    cairosvg = None
    try:
        import cairosvg as _cairosvg

        cairosvg = _cairosvg
    except (ImportError, OSError):
        try:
            subprocess.run(
                [sys.executable, "-m", "pip", "install", "cairosvg", "-q"],
                check=False,
            )
            import cairosvg as _cairosvg

            cairosvg = _cairosvg
        except (ImportError, OSError) as error:
            print(f"WARN: SVG render unavailable ({error}), skip render compare")
            return None

    try:
        png_bytes = cairosvg.svg2png(
            url=str(SVG_PATH),
            output_width=VIEW_W * 2,
            output_height=VIEW_H * 2,
        )
    except OSError as error:
        print(f"WARN: SVG render backend missing ({error}), skip render compare")
        return None

    return Image.open(io.BytesIO(png_bytes)).convert("RGB")


def main() -> int:
    if not SVG_PATH.exists():
        print(f"Missing {SVG_PATH}")
        return 1

    names = [p.name for p in sorted(IMAGES_DIR.glob("*.png"))]
    svg_text = SVG_PATH.read_text(encoding="utf-8")
    embedded = load_embedded_images(svg_text)

    missing = [n for n in names if Path(n).stem.replace(".", "-") not in embedded]
    if missing:
        print("Embedded images missing:", ", ".join(missing))
        return 1

    issues: list[str] = []
    preview_cols = 4
    preview_rows = (len(names) + preview_cols - 1) // preview_cols
    cell_w = CARD_W * 2 + COMPARE_GAP
    cell_h = CARD_H * 2 + 36
    preview = Image.new("RGB", (preview_cols * cell_w, preview_rows * cell_h), "#0d0d0f")
    draw = ImageDraw.Draw(preview)

    for index, name in enumerate(names):
        ident = Path(name).stem.replace(".", "-")
        source = Image.open(IMAGES_DIR / name).convert("RGB")
        src_ratio = source.width / source.height
        target_ratio = CARD_W / CARD_H

        if abs(src_ratio - target_ratio) > 0.001:
            issues.append(f"{name}: source ratio {src_ratio:.4f} != card {target_ratio:.4f}")

        if source.size != (SRC_W, SRC_H):
            issues.append(f"{name}: unexpected size {source.size}, expected {(SRC_W, SRC_H)}")

        expected = expected_card(source)
        actual = card_from_embedded(embedded[ident])
        score = diff_score(expected, actual)
        if score > 6.0:
            issues.append(f"{name}: embed differs from source resize (diff {score:.2f})")

        col = index % preview_cols
        row = index // preview_cols
        ox = col * cell_w
        oy = row * cell_h + 16
        preview.paste(expected, (ox, oy))
        preview.paste(actual, (ox + CARD_W + COMPARE_GAP, oy))
        draw.text((ox, oy - 14), f"{name} source", fill="#aaaaaa")
        draw.text((ox + CARD_W + COMPARE_GAP, oy - 14), "embedded", fill="#aaaaaa")

    PREVIEW_PATH.parent.mkdir(parents=True, exist_ok=True)
    preview.save(PREVIEW_PATH, format="PNG", optimize=True)

    frame = build_frame(names, embedded)
    frame.save(FRAME_PATH, format="PNG", optimize=True)

    rendered = render_svg()
    if rendered is not None:
        rendered = rendered.resize((VIEW_W, VIEW_H), Image.Resampling.LANCZOS)
        rendered.save(SVG_RENDER_PATH, format="PNG", optimize=True)
        render_diff = diff_score(rendered, frame)
        if render_diff > MAX_RENDER_DIFF:
            issues.append(
                f"SVG render differs from frame preview (diff {render_diff:.2f} > {MAX_RENDER_DIFF})"
            )

    if issues:
        print("VERIFY FAILED:")
        for issue in issues:
            print(f"  - {issue}")
        print(f"Preview: {PREVIEW_PATH}")
        print(f"Frame: {FRAME_PATH}")
        if rendered is not None:
            print(f"SVG render: {SVG_RENDER_PATH}")
        return 1

    print(f"VERIFY OK: {len(names)} cards are full {CARD_W}x{CARD_H} (16:10), no crop")
    print(f"Preview: {PREVIEW_PATH}")
    print(f"Frame: {FRAME_PATH}")
    if rendered is not None:
        print(f"SVG render: {SVG_RENDER_PATH}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

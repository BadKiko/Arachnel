#!/usr/bin/env python3
"""Generate Arachnel spider-web icon assets (SVG, PNG, ICO)."""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
ICON_DIR = ROOT / "resources" / "icons"
PNG_DIR = ICON_DIR / "png"
HICOLOR_DIR = ROOT / "resources" / "linux" / "icons" / "hicolor"

SPOKES = 8
RINGS = 3

COLORS = {
    "bg": "#141218",
    "circle": "#E8DEF8",
    "stroke": "#49454F",
}


def hex_to_rgb(value: str) -> tuple[int, int, int]:
    value = value.lstrip("#")
    return tuple(int(value[i : i + 2], 16) for i in (0, 2, 4))


def spoke_angle(index: int) -> float:
    return (index / SPOKES) * math.tau - math.pi / 2


def point(cx: float, cy: float, radius: float, angle: float) -> tuple[float, float]:
    return cx + math.cos(angle) * radius, cy + math.sin(angle) * radius


def ring_points(cx: float, cy: float, radius: float) -> list[tuple[float, float]]:
    return [point(cx, cy, radius, spoke_angle(index)) for index in range(SPOKES)]


def svg_path(points: list[tuple[float, float]]) -> str:
    commands = [f"M {points[0][0]:.2f} {points[0][1]:.2f}"]
    for x, y in points[1:]:
        commands.append(f"L {x:.2f} {y:.2f}")
    commands.append("Z")
    return " ".join(commands)


def build_web_svg(
    *,
    size: int = 512,
    with_square_bg: bool = True,
    with_circle_bg: bool = True,
    transparent: bool = False,
) -> str:
    cx = cy = size / 2
    circle_r = size * 0.39
    max_r = circle_r * 0.9
    stroke = size * 0.028
    hub_r = max(stroke * 1.4, size * 0.024)

    bg = "" if transparent else f'<rect width="{size}" height="{size}" fill="{COLORS["bg"]}"/>'
    if with_square_bg and not transparent:
        radius = size * 0.18
        bg = (
            f'<rect width="{size}" height="{size}" rx="{radius:.1f}" '
            f'fill="{COLORS["bg"]}"/>'
        )

    circle = ""
    if with_circle_bg:
        circle = (
            f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{circle_r:.2f}" '
            f'fill="{COLORS["circle"]}"/>'
        )

    spokes = []
    for index in range(SPOKES):
        x, y = point(cx, cy, max_r, spoke_angle(index))
        spokes.append(
            f'<line x1="{cx:.2f}" y1="{cy:.2f}" x2="{x:.2f}" y2="{y:.2f}"/>'
        )

    rings = []
    for ring in range(1, RINGS + 1):
        rad = max_r * (ring / RINGS)
        rings.append(f'<path d="{svg_path(ring_points(cx, cy, rad))}"/>')

    web = "\n    ".join(spokes + rings)
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {size} {size}" role="img" aria-label="Arachnel">
  {bg}
  {circle}
  <g fill="none" stroke="{COLORS["stroke"]}" stroke-width="{stroke:.2f}"
     stroke-linecap="round" stroke-linejoin="round">
    {web}
  </g>
  <circle cx="{cx:.2f}" cy="{cy:.2f}" r="{hub_r:.2f}" fill="{COLORS["stroke"]}"/>
</svg>
"""


def draw_web(
    draw: ImageDraw.ImageDraw,
    *,
    cx: float,
    cy: float,
    max_r: float,
    stroke_width: float,
    stroke_rgb: tuple[int, int, int],
) -> None:
    for index in range(SPOKES):
        x, y = point(cx, cy, max_r, spoke_angle(index))
        draw.line((cx, cy, x, y), fill=stroke_rgb, width=stroke_width)

    for ring in range(1, RINGS + 1):
        rad = max_r * (ring / RINGS)
        points = ring_points(cx, cy, rad)
        draw.polygon(points, outline=stroke_rgb, width=stroke_width)

    hub_r = max(stroke_width, int(max_r * 0.04))
    draw.ellipse(
        (cx - hub_r, cy - hub_r, cx + hub_r, cy + hub_r),
        fill=stroke_rgb,
    )


def render_png(size: int, *, transparent: bool = False) -> Image.Image:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0) if transparent else hex_to_rgb(COLORS["bg"]) + (255,))
    draw = ImageDraw.Draw(image)

    cx = cy = size / 2
    circle_r = size * 0.39
    max_r = circle_r * 0.9
    stroke_width = max(1, round(size * 0.028))

    if not transparent:
        radius = round(size * 0.18)
        draw.rounded_rectangle((0, 0, size - 1, size - 1), radius=radius, fill=hex_to_rgb(COLORS["bg"]))

    draw.ellipse(
        (cx - circle_r, cy - circle_r, cx + circle_r, cy + circle_r),
        fill=hex_to_rgb(COLORS["circle"]),
    )
    draw_web(
        draw,
        cx=cx,
        cy=cy,
        max_r=max_r,
        stroke_width=stroke_width,
        stroke_rgb=hex_to_rgb(COLORS["stroke"]),
    )
    return image


def write_ico(path: Path, sizes: list[int]) -> None:
    images = [render_png(size) for size in sizes]
    images[0].save(path, format="ICO", sizes=[(size, size) for size in sizes])


def main() -> None:
    ICON_DIR.mkdir(parents=True, exist_ok=True)
    PNG_DIR.mkdir(parents=True, exist_ok=True)

    (ICON_DIR / "arachnel.svg").write_text(
        build_web_svg(with_square_bg=True, with_circle_bg=True, transparent=False),
        encoding="utf-8",
    )
    (ICON_DIR / "arachnel-github.svg").write_text(
        build_web_svg(with_square_bg=False, with_circle_bg=True, transparent=True),
        encoding="utf-8",
    )

    png_sizes = [16, 22, 24, 32, 48, 64, 128, 256, 512]
    for size in png_sizes:
        render_png(size).save(PNG_DIR / f"{size}.png", format="PNG")

        apps_dir = HICOLOR_DIR / f"{size}x{size}" / "apps"
        apps_dir.mkdir(parents=True, exist_ok=True)
        render_png(size).save(apps_dir / "arachnel.png", format="PNG")

    write_ico(ICON_DIR / "arachnel.ico", [16, 24, 32, 48, 64, 128, 256])

    print(f"Wrote icons under {ICON_DIR}")
    print(f"Wrote Linux hicolor icons under {HICOLOR_DIR}")


if __name__ == "__main__":
    main()

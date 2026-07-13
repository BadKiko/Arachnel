#!/usr/bin/env python3
"""Generate arachnel_ids.ts for qsTrId English fallback (lrelease -idbased)."""

from __future__ import annotations

import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EN_TS = ROOT / "translations" / "arachnel_en.ts"


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "translations" / "arachnel_ids.ts"
    if not EN_TS.exists():
        print(f"Missing {EN_TS}", file=sys.stderr)
        return 1

    tree = ET.parse(EN_TS)
    ids_root = ET.Element("TS", version="2.1", language="en_US")
    ctx = ET.SubElement(ids_root, "context")
    ET.SubElement(ctx, "name").text = ""

    count = 0
    for msg in tree.getroot().iter("message"):
        mid = msg.attrib.get("id")
        if not mid:
            continue
        source_el = msg.find("source")
        src_text = (source_el.text or "") if source_el is not None else ""
        if not src_text.strip():
            continue

        new_msg = ET.SubElement(ctx, "message", id=mid)
        ET.SubElement(new_msg, "source").text = src_text
        ET.SubElement(new_msg, "translation").text = src_text
        count += 1

    out.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(ids_root).write(out, encoding="utf-8", xml_declaration=True)
    print(f"Wrote {out} ({count} id-based strings)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

# -*- coding: utf-8 -*-
"""Fill arachnel_en.ts with identity translations (source == translation)."""

from __future__ import annotations

import pathlib
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parents[1]
EN_TS = ROOT / "translations" / "arachnel_en.ts"


def source_text(element: ET.Element | None) -> str:
    if element is None:
        return ""
    return "".join(element.itertext())


def main() -> None:
    tree = ET.parse(EN_TS)
    root = tree.getroot()
    filled = 0

    for message in root.iter("message"):
        source_el = message.find("source")
        translation_el = message.find("translation")
        if source_el is None or translation_el is None:
            continue
        translation_el.text = source_text(source_el)
        if "type" in translation_el.attrib:
            del translation_el.attrib["type"]
        filled += 1

    ET.indent(root, space="    ")
    tree.write(str(EN_TS), encoding="utf-8", xml_declaration=True)
    print(f"Filled {filled} identity translations in {EN_TS.name}")


if __name__ == "__main__":
    main()

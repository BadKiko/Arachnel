# -*- coding: utf-8 -*-
"""Fill arachnel_ru.ts from the English->Russian map used during qsTr conversion."""

from __future__ import annotations

import importlib.util
import pathlib
import re
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parents[1]
RU_TS = ROOT / "translations" / "arachnel_ru.ts"

spec = importlib.util.spec_from_file_location(
    "convert", ROOT / "tools" / "convert_qsTr_to_english.py")
convert = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(convert)

EN_TO_RU = {en: ru for ru, en in convert.MAP.items()}
EN_TO_RU.update({
    "Install failed": "Ошибка установки",
    "Description": "Описание",
    "Information": "Информация",
    "Source": "Источник",
    "Version": "Версия",
    "Size": "Размер",
    "—": "—",
    "Download": "Загрузить",
    "v%1 · %2": "v%1 · %2",
    "URL games.json": "URL games.json",
    "Material 3 theme and palette apply across the app.": "Тема и палитра Material 3 применяются ко всему приложению.",
    "Dark theme": "Тёмная тема",
    "Light theme": "Светлая тема",
    "Palette": "Палитра",
    "Language": "Язык",
    "Community translations": "Переводы сообщества",
    "Missing your language? Help translate Arachnel on Weblate or send a pull request with translations/*.ts files.": "Нет вашего языка? Помогите перевести Arachnel на Weblate или отправьте pull request с файлами translations/*.ts.",
    "Help translate": "Помочь с переводом",
    "Theme, palette, accent color, and language": "Тема, палитра, акцентный цвет и язык",
    "Install the .arach package using the button below.\n\nAfter building, FreeTP is in dist:\nbuild-win/dist/freetp.arach": "Установите пакет .arach через кнопку ниже.\n\nПосле сборки FreeTP лежит в dist:\nbuild-win/dist/freetp.arach",
})


def main() -> None:
    text = RU_TS.read_text(encoding="utf-8")
    tree = ET.ElementTree(ET.fromstring(text))
    root = tree.getroot()
    filled = 0
    missing: list[str] = []

    for message in root.iter("message"):
        source_el = message.find("source")
        translation_el = message.find("translation")
        if source_el is None or translation_el is None:
            continue
        source = source_el.text or ""
        if source in EN_TO_RU:
            translation_el.text = EN_TO_RU[source]
            if "type" in translation_el.attrib:
                del translation_el.attrib["type"]
            filled += 1
        elif source not in ("Arachnel", "Primary", "DLC", "FreeTP", "English", "Russian"):
            missing.append(source)

    ET.indent(tree, space="    ")
    tree.write(RU_TS, encoding="utf-8", xml_declaration=True)
    print(f"Filled {filled} translations, {len(missing)} without Russian map")
    for item in missing[:20]:
        print(f"  - {item}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Normalize Qt .ts files for Weblate and lrelease.

- arachnel_en.ts: monolingual SOURCE catalog (no <translation> tags).
- arachnel_ru.ts: bilingual target with Russian translations filled where known.
"""

from __future__ import annotations

import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EN_TS = ROOT / "translations" / "arachnel_en.ts"
RU_TS = ROOT / "translations" / "arachnel_ru.ts"

# English source -> Russian translation for Core + common UI.
EN_TO_RU: dict[str, str] = {
    "Arachnel": "Arachnel",
    "Primary": "Основной",
    "Game": "Игра",
    "Add-on": "Дополнение",
    "Component": "Компонент",
    "Direct": "Прямая",
    "Torrent": "Торрент",
    "Download": "Загрузка",
    "Portable": "Портабл",
    "Installer": "Установщик",
    "Bundled fix": "Встроенный фикс",
    "Separate fix": "Отдельный фикс",
    "Unknown": "Неизвестно",
    "Add-on %1 — %2": "Дополнение %1 — %2",
    "Install failed: %1": "Ошибка установки: %1",
    "Installing (%1/%2)": "Установка (%1/%2)",
    "Error: %1": "Ошибка: %1",
    "Install": "Установка",
    "Update": "Обновление",
    "Task": "Задача",
    "Queued": "В очереди",
    "Starting": "Запуск",
    "Checking": "Проверка",
    "Metadata": "Метаданные",
    "Downloading": "Загрузка",
    "Installing": "Установка",
    "Seeding": "Раздача",
    "Paused": "Пауза",
    "Completed": "Завершено",
    "Failed": "Ошибка",
    "Cancelled": "Отменено",
    "Install failed": "Ошибка установки",
    "Installed": "Установлено",
    "Download complete": "Загрузка завершена",
    "Installation required": "Требуется установка",
    "Downloading…": "Загрузка…",
    "Connecting…": "Подключение…",
    "Installing…": "Установка…",
    "Installing add-on…": "Установка дополнения…",
    "Resuming…": "Возобновление…",
    "Failed to start torrent": "Не удалось начать торрент",
    "Failed to start HTTP download": "Не удалось начать HTTP-загрузку",
    "Downloading %1": "Загрузка %1",
    "Updating %1": "Обновление %1",
    "Catalog error: %1": "Ошибка каталога: %1",
    "Game not found for add-on": "Игра не найдена для дополнения",
    "Add-on not found in catalog": "Дополнение не найдено в каталоге",
    "Could not find game to install: %1": "Не удалось найти игру для установки: %1",
    "Download error: %1": "Ошибка загрузки: %1",
    "Installation of %1 is already in progress": "Установка %1 уже выполняется",
    "Source plugin not found: %1": "Плагин источника не найден: %1",
    "No install handler for %1": "Нет обработчика установки для %1",
    "Download complete — install manually": "Загрузка завершена — установите вручную",
    "Automatic install is unavailable. Choose the folder where the game is installed.": "Автоустановка недоступна. Укажите папку, куда установлена игра.",
    "Automatic install is unavailable. Run setup.exe from the download folder, then use the folder button to point to the game.": "Автоустановка недоступна. Запустите setup.exe из папки загрузки, затем кнопкой папки укажите, куда установилась игра.",
    "Choose game install folder": "Выберите папку с игрой",
    "No game executable found in %1": "В %1 не найден исполняемый файл игры",
    "Installed": "Установлено",
    "Manual install complete for %1": "Ручная установка завершена: %1",
    "Install failed for %1: %2": "Ошибка установки %1: %2",
    "Update installed: %1": "Обновление установлено: %1",
    "Installed: %1": "Установлено: %1",
    "Add-on installation is already in progress": "Установка дополнения уже выполняется",
    "Install the game first": "Сначала установите игру",
    "Add-on install failed for %1: %2": "Ошибка установки дополнения %1: %2",
    "Add-on installed: %1": "Дополнение установлено: %1",
    "Game not found": "Игра не найдена",
    "Game settings": "Настройки игры",
    "Application crashed": "Приложение аварийно завершилось",
    "Arachnel stopped unexpectedly during the last session.": "Arachnel неожиданно завершилось в прошлой сессии.",
    "Arachnel has crashed.": "Arachnel аварийно завершилось.",
    "Source page": "Страница на источнике",
    "Source website": "Сайт источника",
    "Steam": "Steam",
    "Gameplay video": "Видео геймплея",
    "Screenshots": "Скриншоты",
    "Screenshot %1 of %2": "Скриншот %1 из %2",
    "Open in browser": "Открыть в браузере",
    "Close": "Закрыть",
    "Report file: %1": "Файл отчёта: %1",
    "Open folder": "Открыть папку",
    "Copy report": "Копировать отчёт",
    "Create GitHub issue": "Создать issue на GitHub",
    "Dismiss": "Закрыть",
    "Add-on not found": "Дополнение не найдено",
    "Download the add-on first": "Сначала скачайте дополнение",
    "%1 update(s) available": "Доступно обновлений: %1",
    "No updates": "Обновлений нет",
    "Game not installed": "Игра не установлена",
    "Install folder not found": "Папка установки не найдена",
    "Executable not found": "Исполняемый файл не найден",
    "Executable is missing": "Исполняемый файл отсутствует",
    "%1 · %2 games": "%1 · %2 игр",
    "%1 sources · %2 games": "%1 источников · %2 игр",
    "Catalog empty or unavailable: %1": "Каталог пуст или недоступен: %1",
    "No catalog URL configured for source %1": "Для источника %1 не задан URL каталога",
    "Game not found: %1": "Игра не найдена: %1",
    "%1 is not installed yet": "%1 ещё не установлена",
    "%1 is already running": "%1 уже запущена",
    "Executable not found for %1": "Не найден исполняемый файл для %1",
    "Failed to launch game": "Не удалось запустить игру",
    "Failed to stop game": "Не удалось остановить игру",
    "Unknown source: %1": "Неизвестный источник: %1",
    'Source "%1" is disabled in settings': "Источник «%1» выключен в настройках",
    "Enter a catalog URL": "Укажите URL каталога",
    "Invalid URL — http or https required": "Некорректный URL — нужен http или https",
    "Catalog entry not found: %1": "Запись каталога не найдена: %1",
    "No magnet link for %1": "Нет magnet-ссылки для %1",
    "No magnet link": "Нет magnet-ссылки",
    "Torrent error %1": "Ошибка торрента %1",
    "Could not start download for %1": "Не удалось начать загрузку %1",
    "Folder picker is only available on Windows": "Выбор папки пока доступен только в Windows",
    "Game not found in library": "Игра не найдена в библиотеке",
    "Game removed: %1": "Игра удалена: %1",
    "No destination library selected": "Не выбран диск назначения",
    "Game is already on this library": "Игра уже на этом диске",
    "Could not move: %1": "Не удалось перенести: %1",
    "Game moved: %1": "Игра перенесена: %1",
    "Could not start add-on download": "Не удалось начать загрузку дополнения",
    "Entry not found: %1": "Запись не найдена: %1",
    "Could not start update for %1": "Не удалось начать обновление %1",
    "No catalog sources enabled": "Нет включённых источников каталога",
    "Download not found": "Загрузка не найдена",
    "Installation is only available for completed downloads": "Установка доступна только для завершённых загрузок",
    "Add-on file not found": "Файл дополнения не найден",
    "Download files not found": "Файлы загрузки не найдены",
    "Source plugin not found": "Плагин источника не найден",
    "Could not find game to install": "Не удалось найти игру для установки",
    "Plugin installed": "Плагин установлен",
    "Invalid plugin package: expected a ZIP .arach archive":
        "Некорректный пакет плагина: ожидается ZIP-архив .arach",
    "Could not start archive extraction": "Не удалось запустить распаковку архива",
    "Could not create temporary folder": "Не удалось создать временную папку",
    "Could not prepare plugin archive for extraction":
        "Не удалось подготовить архив плагина к распаковке",
    "Archive extraction timed out": "Таймаут распаковки",
    "Archive extraction failed (code %1)": "Ошибка распаковки (код %1)",
    "Plugin files were copied but the library failed to load. Rebuild the plugin for "
    "your Arachnel version and platform (MSVC/MinGW), then reinstall.":
        "Плагин скопирован, но библиотека не загрузилась. Пересоберите плагин под вашу "
        "версию Arachnel и платформу (MSVC/MinGW) и установите снова.",
    "Plugin install failed: %1": "Ошибка установки плагина: %1",
    "File picker is only available on Windows": "Выбор файла пока доступен только в Windows",
    "Could not open plugins folder": "Не удалось открыть папку плагинов",
    "Choose library folder": "Выберите папку библиотеки",
    "Plugin package (*.arach)": "Пакет плагина (*.arach)",
    "Nothing played yet": "Ещё ничего не играли",
    "Launch a game from your library — it will appear here.": "Запустите игру из библиотеки — она появится здесь.",
    "Proton-GE installed": "Proton-GE установлен",
    "Proton-GE download failed: %1": "Ошибка загрузки Proton-GE: %1",
    "Choose game executable": "Выберите исполняемый файл игры",
    "Executables (*.exe *.sh *.x86_64);;All files (*)": "Исполняемые (*.exe *.sh *.x86_64);;Все файлы (*)",
    "Choose Proton executable": "Выберите исполняемый файл Proton",
    "Proton (proton);;All files (*)": "Proton (proton);;Все файлы (*)",
    "Proton not found. Install Proton-GE in Settings → Launch.": "Proton не найден. Установите Proton-GE в Настройки → Запуск.",
    "Install Proton-GE in Settings → Launch before downloading games": "Установите Proton-GE в Настройки → Запуск перед скачиванием игр",
    "Install %1 (Proton-GE) in Settings → Launch before downloading games": "Установите %1 (Proton-GE) в Настройки → Запуск перед скачиванием игр",
    "Launch options": "Параметры запуска",
    "Extra launch arguments for this game": "Доп. аргументы запуска для этой игры",
    "Custom executable (optional)": "Свой исполняемый файл (необязательно)",
    "Launch": "Запуск",
    "Global arguments and Proton-GE on Linux": "Глобальные аргументы и Proton-GE на Linux",
    "Extra command-line arguments appended to every game launch.": "Дополнительные аргументы командной строки для каждого запуска.",
    "Global launch arguments": "Глобальные аргументы запуска",
    "Linux: all games run through Proton (Windows builds).": "Linux: все игры запускаются через Proton (Windows-сборки).",
    "Proton runtime": "Среда Proton",
    "Active: %1": "Активная версия: %1",
    "Required before download: %1": "Нужна перед скачиванием: %1",
    "Install Proton-GE before downloading games.": "Установите Proton-GE перед скачиванием игр.",
    "Auto-detect from installed Proton-GE or Steam": "Авто: установленный Proton-GE или Steam",
    "Download Proton-GE": "Скачать Proton-GE",
    "Download %1": "Скачать %1",
    "Browse…": "Обзор…",
    "Clear": "Сбросить",
    "Installed: %1": "Установлено: %1",
    "Proton required": "Нужен Proton",
    "latest Proton-GE": "последний Proton-GE",
    "Games run through Proton on Linux. Install %1 before downloading.": "На Linux игры запускаются через Proton. Установите %1 перед скачиванием.",
    "Games run through Proton on Linux. Install Proton-GE before downloading.": "На Linux игры запускаются через Proton. Установите Proton-GE перед скачиванием.",
    "Currently installed: %1": "Сейчас установлено: %1",
    "Settings": "Настройки",
    "Downloading…": "Загрузка…",
    "Default: %1": "По умолчанию: %1",
    "Pick default Proton and drag priority with arrows. Steam installs are detected automatically.":
        "Выберите Proton по умолчанию и приоритет стрелками. Установки Steam подхватываются автоматически.",
    "Rescan": "Пересканировать",
    "No Proton found. Download Proton-GE or install Proton in Steam.":
        "Proton не найден. Скачайте Proton-GE или установите Proton в Steam.",
    "Override Proton for this game. Default uses Settings → Launch.":
        "Свой Proton для этой игры. По умолчанию — из Настройки → Запуск.",
    "Для установки Windows-установщика нужен Proton (Настройки → Запуск)":
        "Для установки Windows-установщика нужен Proton (Настройки → Запуск)",
    "About": "О приложении",
    "Application name, version, and platform": "Название, версия и платформа",
    "Game launcher with plugin-based sources and Hydra catalogs.":
        "Лаунчер игр с плагинами источников и каталогами Hydra.",
    "Application": "Приложение",
    "Version": "Версия",
    "Platform": "Платформа",
    "Unknown": "Неизвестно",
    "Windows": "Windows",
    "Linux": "Linux",
    "macOS": "macOS",
    "Appearance": "Оформление",
    "Theme, palette, accent color, and language": "Тема, палитра, акцент и язык",
}


def parse_ts(path: Path) -> ET.ElementTree:
    return ET.parse(path)


def strip_translations_from_en(tree: ET.ElementTree) -> int:
    removed = 0
    for message in tree.getroot().iter("message"):
        translation = message.find("translation")
        if translation is not None:
            message.remove(translation)
            removed += 1
    return removed


def fill_ru_translations(tree: ET.ElementTree) -> tuple[int, list[str]]:
    filled = 0
    missing: list[str] = []
    for message in tree.getroot().iter("message"):
        source_el = message.find("source")
        translation_el = message.find("translation")
        if source_el is None or translation_el is None:
            continue
        source = source_el.text or ""
        if not source:
            continue

        existing = translation_el.text or ""
        is_unfinished = translation_el.attrib.get("type") == "unfinished"
        if existing.strip() and not is_unfinished:
            continue

        # lupdate may copy a translation but leave type="unfinished" — finalize it.
        if existing.strip() and is_unfinished:
            translation_el.attrib.pop("type", None)
            filled += 1
            continue

        ru = EN_TO_RU.get(source)
        if ru is None:
            if is_unfinished or not existing.strip():
                missing.append(source)
            continue

        translation_el.text = ru
        translation_el.attrib.pop("type", None)
        filled += 1
    return filled, missing


def write_ts(tree: ET.ElementTree, path: Path, language: str) -> None:
    root = tree.getroot()
    root.set("version", "2.1")
    root.set("language", language)
    xml = ET.tostring(root, encoding="unicode")
    xml = re.sub(r"<location filename=", r"\n        <location filename=", xml)
    xml = re.sub(r"</context>", r"\n</context>", xml)
    header = '<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>\n'
    path.write_text(header + xml + "\n", encoding="utf-8")


def main() -> int:
    if not EN_TS.exists() or not RU_TS.exists():
        print("Missing translations/*.ts", file=sys.stderr)
        return 1

    en_tree = parse_ts(EN_TS)
    ru_tree = parse_ts(RU_TS)

    removed = strip_translations_from_en(en_tree)
    filled, missing = fill_ru_translations(ru_tree)

    write_ts(en_tree, EN_TS, "en_US")
    write_ts(ru_tree, RU_TS, "ru_RU")

    print(f"arachnel_en.ts: removed {removed} <translation> tags (monolingual template)")
    print(f"arachnel_ru.ts: filled {filled} known translations")
    if missing:
        print(f"arachnel_ru.ts: {len(missing)} still without mapping (kept existing or unfinished)")
        for item in missing[:10]:
            print(f"  - {item[:80]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

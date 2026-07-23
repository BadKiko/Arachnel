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
    "Failed to start: %1": "Не удалось запустить: %1",
    "Timeout: %1": "Таймаут: %1",
    "%1 exited with code %2": "%1 завершился с кодом %2",
    "launch cancelled (UAC)": "запуск отменён (UAC)",
    "administrator rights required": "требуются права администратора",
    "File not found: %1": "Файл не найден: %1",
    "Failed to start %1: %2": "Не удалось запустить %1: %2",
    "Could not track installer process": "Не удалось отследить процесс установки",
    "Executable is not set": "Исполняемый файл не задан",
    "Failed to start process": "Не удалось запустить процесс",
    "Failed to delete file: %1": "Не удалось удалить файл: %1",
    "Failed to delete folder: %1": "Не удалось удалить папку: %1",
    "Source not found: %1": "Источник не найден: %1",
    "Failed to replace: %1": "Не удалось заменить: %1",
    "Failed to copy: %1": "Не удалось скопировать: %1",
    "Failed to create folder: %1": "Не удалось создать папку: %1",
    "Plugin not found": "Плагин не найден",
    "Disk": "Диск",
    "Failed to create temporary folder": "Не удалось создать временную папку",
    "Failed to read plugin.json": "Не удалось прочитать plugin.json",
    "Failed to replace existing plugin": "Не удалось заменить существующий плагин",
    "Failed to create plugin folder": "Не удалось создать папку плагина",
    "Failed to copy %1": "Не удалось скопировать %1",
    "Failed to replace %1": "Не удалось заменить %1",
    "Failed to save file": "Не удалось сохранить файл",
    "Failed to parse games from catalog": "Не удалось разобрать игры из каталога",
    "Only .arach packages are supported": "Поддерживаются только пакеты с расширением .arach",
    "Archive has no plugin.json": "В архиве нет plugin.json",
    "Invalid plugin.json": "Некорректный plugin.json",
    "Package is missing library %1": "В пакете нет библиотеки %1",
    "Empty server response": "Пустой ответ сервера",
    "Invalid JSON": "Некорректный JSON",
    "No downloads array — not a Hydra catalog": "Нет массива downloads — это не каталог Hydra",
    "downloads array is empty": "Массив downloads пуст",
    "Catalog is empty or format not recognized": "Каталог пуст или формат не распознан",
    "FreeTP torrent catalog — magnet links and add-ons": "Торрент-каталог FreeTP — magnet-ссылки и дополнения",
    "Choose library folder": "Выберите папку библиотеки",
    "Remember choice": "Запомнить выбор",
    "Continue": "Продолжить",
    "Got it": "Понятно",
    "Done": "Готово",
    "Steam CDN + Online Fix": "Steam CDN + Online Fix",
    "Steam CDN · Online Fix": "Steam CDN · Online Fix",
    "Ready to download from Steam CDN. Online Fix can be included when needed.": "Готово к загрузке с Steam CDN. При необходимости можно включить Online Fix.",
    "Downloading from Steam CDN…": "Загрузка с Steam CDN…",
    "Unknown download": "Неизвестная загрузка",
    "Clear finished": "Очистить завершённые",
    "%1 active · resume after restart": "%1 активных · продолжатся после перезапуска",
    "%1 active · %2 finished · resume after restart": "%1 активных · %2 завершено · продолжатся после перезапуска",
    "%1 finished · torrents resume after restart": "%1 завершено · торренты продолжатся после перезапуска",
    "Torrents resume after restart": "Торренты докачиваются после перезапуска",
    "No downloads": "Нет загрузок",
    "Scan for installed games": "Найти установленные игры",
    "Found %1 game(s) on disk": "Найдено игр на диске: %1",
    "No new games found on disk": "Новых игр на диске не найдено",
    "Add drive…": "Добавить диск…",
    "Remove drive?": "Удалить диск?",
    "Remove “%1” from Arachnel? Files on disk are not deleted.":
        "Убрать «%1» из Arachnel? Файлы на диске не удаляются.",
    "“%1” still has games (%2). Remove the drive from Arachnel anyway? Files stay on disk; games stay in the library under another drive.":
        "На «%1» ещё есть игры (%2). Убрать диск из Arachnel всё равно? Файлы на диске останутся; игры останутся в библиотеке на другом диске.",
    "Remove anyway": "Убрать всё равно",
    "Drive removed": "Диск убран",
    "Drive removed. %1 game(s) kept on disk and listed under another drive.":
        "Диск убран. Игр оставлено на диске и в библиотеке (на другом диске): %1",
    "Remove": "Убрать",
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
    "Installing %1": "Установка %1",
    "Updating %1": "Обновление %1",
    "Preparing…": "Подготовка…",
    "Preparing to play": "Подготовка к запуску",
    "Preparing Steam…": "Подготовка Steam…",
    "Getting game info…": "Получение данных об игре…",
    "Finishing…": "Завершение…",
    "This game is not available for download right now. Try another title.": "Эта игра сейчас недоступна для загрузки. Попробуйте другую.",
    "Could not prepare this game for download. Try again later or pick another title.": "Не удалось подготовить игру к загрузке. Попробуйте позже или выберите другую.",
    "Completed": "Завершено",
    "Download failed. Try again or pick another game.": "Ошибка загрузки. Попробуйте снова или выберите другую игру.",
    "Steam blocked downloading game files (need a packaged manifest). Try another title, or set hubcapApiKey in plugin settings.": "Steam заблокировал загрузку файлов (нужен готовый манифест). Попробуйте другую игру или укажите hubcapApiKey в настройках плагина.",
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
    "Online Fix": "Online Fix",
    "Online Fix for this game": "Online Fix для этой игры",
    "When disabled, SteamFix/winmm overlay DLLs are renamed so the game runs without the fix.": "Если выключить, DLL оверлея SteamFix/winmm переименуются — игра запустится без фикса.",
    "Not installed": "Не установлен",
    "Not needed": "Не нужен",
    "Enabled": "Включён",
    "Disabled": "Отключён",
    "Online Fix overlay not found in this install": "Online Fix оверлей не найден в этой установке",
    "Failed to enable Online Fix: %1": "Не удалось включить Online Fix: %1",
    "Failed to disable Online Fix: %1": "Не удалось отключить Online Fix: %1",
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
    "Removing “%1”…": "Удаление «%1»…",
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
    "Plugin install failed": "Ошибка установки плагина",
    "v%1": "v%1",
    "v%1 · %2": "v%1 · %2",
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
    "Steam App ID is missing": "Не указан Steam App ID",
    "Could not write file: %1": "Не удалось записать файл: %1",
    "Could not start installer: %1": "Не удалось запустить установщик: %1",
    "Installer timed out: %1": "Таймаут установщика: %1",
    "Installer failed (%1): %2": "Ошибка установщика (%1): %2",
    "No installer mapping for %1": "Нет установщика для %1",
    "Installing runtime: %1": "Установка среды: %1",
    "Downloading runtime: %1": "Загрузка среды: %1",
    "Installer not found for %1": "Установщик не найден для %1",
    "Preparing runtime environment…": "Подготовка среды выполнения…",
    "Runtime setup is already in progress": "Установка среды уже выполняется",
    "Proton is required to install runtime dependencies": "Для установки зависимостей нужен Proton",
    "Runtime container": "Контейнер среды",
    "Proton prefix and redistributables for this game (Linux only).": "Prefix Proton и redistributables для этой игры (только Linux).",
    "Container": "Контейнер",
    "Prefix": "Prefix",
    "%1 (not created yet)": "%1 (ещё не создан)",
    "Steam App ID": "Steam App ID",
    "No runtime dependencies detected for this game.": "Для этой игры зависимости среды не найдены.",
    "Dependencies: %1 / %2 installed": "Зависимости: %1 / %2 установлено",
    "Installed": "Установлено",
    "Missing": "Нет",
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
    "Not checked yet": "Еще не проверялось",
    "Checking for Arachnel updates…": "Проверка обновлений Arachnel…",
    "Update check failed: %1": "Ошибка проверки обновлений: %1",
    "Could not parse GitHub release information": "Не удалось разобрать информацию о релизе GitHub",
    "Arachnel %1 is available": "Доступен Arachnel %1",
    "Update available": "Доступно обновление",
    "Arachnel %1 is ready to install. Update now to get the latest fixes and features.":
        "Arachnel %1 готов к установке. Обновитесь, чтобы получить последние исправления и возможности.",
    "Later": "Позже",
    "Release page": "Страница релиза",
    "Update now": "Обновить",
    "Arachnel is up to date (%1)": "Arachnel актуален (%1)",
    "Update found, but no installer package is available for this platform":
        "Обновление найдено, но для этой платформы нет установщика",
    "Open the release page to download the latest package for your platform":
        "Откройте страницу релиза и скачайте пакет для вашей платформы",
    "Downloading Arachnel update…": "Скачивание обновления Arachnel…",
    "Download failed: %1": "Ошибка загрузки: %1",
    "Could not save the downloaded installer": "Не удалось сохранить скачанный установщик",
    "Starting updater…": "Запуск обновления…",
    "Could not start the Arachnel installer": "Не удалось запустить установщик Arachnel",
    "Automatic installer launch is only available on Windows":
        "Автозапуск установщика доступен только на Windows",
    "Please wait — updating Arachnel…": "Подождите — обновление Arachnel…",
    "Waiting for Arachnel to close…": "Ожидание закрытия Arachnel…",
    "Arachnel is still running. Close it and try again.":
        "Arachnel ещё запущен. Закройте его и попробуйте снова.",
    "Updating uninstaller…": "Обновление деинсталлятора…",
    "Refreshing shortcuts…": "Обновление ярлыков…",
    "Update complete": "Обновление завершено",
    "Please wait. The installer will open automatically.":
        "Подождите. Установщик откроется автоматически.",
    "Game catalog updates and Arachnel launcher updates from GitHub.":
        "Обновления игр в каталоге и обновления лаунчера Arachnel с GitHub.",
    "Games": "Игры",
    "Check for game updates": "Проверить обновления игр",
    "Current version: %1": "Текущая версия: %1",
    "Check for Arachnel updates on startup":
        "Проверять обновления Arachnel при запуске",
    "Looks up the latest GitHub release in the background.":
        "В фоне смотрит последний релиз на GitHub.",
    "Check for Arachnel updates": "Проверить обновления Arachnel",
    "Download and install": "Скачать и установить",
    "Open release page": "Открыть страницу релиза",
    "Updating Arachnel…": "Обновление Arachnel…",
    "Please wait while Arachnel is updated. Do not close this window.":
        "Подождите, пока Arachnel обновится. Не закрывайте это окно.",
    "Arachnel is being installed on your computer.":
        "Arachnel устанавливается на ваш компьютер.",
    "Arachnel is up to date": "Arachnel обновлён",
    "Choose language": "Выберите язык",
    "Select the installer language.": "Выберите язык установщика.",
    "Install Arachnel": "Установка Arachnel",
    "Game launcher with plugin-based sources. This wizard unpacks Arachnel to your computer.":
        "Лаунчер игр с плагинами источников. Мастер распакует Arachnel на компьютер.",
    "Choose install location": "Выберите папку установки",
    "Install folder": "Папка установки",
    "Shortcuts": "Ярлыки",
    "Create desktop shortcut": "Ярлык на рабочем столе",
    "Create Start Menu shortcut": "Ярлык в меню Пуск",
    "Arachnel is ready": "Arachnel готов",
    "Continue": "Далее",
    "Finish": "Готово",
    "Finalizing…": "Завершение…",
    "No embedded app payload found. Build the installer with run.ps1 --installer.":
        "Нет встроенного пакета приложения. Соберите установщик через run.ps1 --installer.",
    "Extracting files…": "Распаковка файлов…",
    "Preparing…": "Подготовка…",
    "Registering uninstaller…": "Регистрация деинсталлятора…",
    "Creating shortcuts…": "Создание ярлыков…",
    "Installation complete": "Установка завершена",
    "Clearing install folder…": "Очистка папки установки…",
    "Could not clear existing install folder": "Не удалось очистить папку установки",
    "Creating install folder…": "Создание папки установки…",
    "Could not create install folder": "Не удалось создать папку установки",
    "Could not resolve application data folder": "Не удалось определить папку данных приложения",
    "Failed to delete application data": "Не удалось удалить данные приложения",
    "Failed to reset application data": "Не удалось сбросить данные приложения",
    "Application data deleted. Arachnel will quit now.": "Данные приложения удалены. Arachnel сейчас закроется.",
    "Danger zone": "Опасная зона",
    "Delete application data": "Удалить данные приложения",
    "Removes settings, library metadata, download jobs, cover cache, installed plugins, and Proton builds from the app data folder. Installed game files in your library folders are not deleted. Arachnel will quit afterward.": "Удаляет настройки, метаданные библиотеки, задачи загрузок, кэш обложек, установленные плагины и сборки Proton из папки данных приложения. Файлы игр в папках библиотек не трогаются. После этого Arachnel закроется.",
    "Deletes settings, download history, caches, plugins, and Proton from the app folder. Game files on your disks stay. Arachnel will quit afterward.": "Удаляет настройки, историю загрузок, кэши, плагины и Proton из папки приложения. Файлы игр на дисках остаются. После этого Arachnel закроется.",
    "FreeTP and others — install, launch, and add-ons": "FreeTP и другие — установка, запуск и дополнения",
    "Catalog links — import from Hydra or elsewhere": "Ссылки на каталоги — из Hydra или других источников",
    "Game and launcher updates": "Обновления игр и лаунчера",
    "Launch options and Proton on Linux": "Параметры запуска и Proton на Linux",
    "Theme, colors, and language": "Тема, цвета и язык",
    "Version and app data": "Версия и данные приложения",
    "Browse catalogs, download games, and launch from your library.": "Каталоги, загрузка игр и запуск из библиотеки.",
    "Theme and colors apply across the app.": "Тема и цвета применяются ко всему приложению.",
    "Plugin store": "Магазин плагинов",
    "Installed plugins": "Установленные плагины",
    "No plugins installed": "Нет установленных плагинов",
    "Open the plugin store or install a plugin file you already have.": "Откройте магазин плагинов или установите файл плагина, если он уже есть.",
    "Installed": "Установлен",
    "Repair": "Починить",
    "Install from file…": "Установить из файла…",
    "Remove plugin?": "Удалить плагин?",
    "Remove \"%1\"? Catalogs from this plugin will stop working until you install it again.": "Удалить «%1»? Каталоги этого плагина перестанут работать, пока вы не установите его снова.",
    "Plugin removed": "Плагин удалён",
    "Could not remove plugin: %1": "Не удалось удалить плагин: %1",
    "Downloading Arachnel update…": "Скачивание обновления Arachnel…",
    "Please wait. The installer will open automatically.":
        "Подождите. Установщик откроется автоматически.",
    "Starting…": "Запуск…",
    "Downloading… %1%": "Скачивание… %1%",
    "Installing plugin…": "Установка плагина…",
    "Installing plugin “%1”…": "Установка плагина «%1»…",
    "Downloading and unpacking. The UI stays responsive — please wait.":
        "Скачивание и распаковка. Интерфейс не зависает — подождите.",
    "Plugins updated": "Плагины обновлены",
    "v%1 · %2 — not loaded": "v%1 · %2 — не загружен",
    "Invalid plugin id": "Некорректный id плагина",
    "Plugin is not installed": "Плагин не установлен",
    "Failed to install plugin files": "Не удалось установить файлы плагина",
    "Failed to replace existing plugin": "Не удалось заменить установленный плагин",
    "Official plugins from the Arachnel catalog. Install adds them to your plugins folder.": "Официальные плагины из каталога Arachnel. Установка кладёт их в папку плагинов.",
    "Available": "Доступные",
    "No plugins found": "Плагины не найдены",
    "Install plugin…": "Установить плагин…",
    "Delete": "Удалить",
    "Cancel": "Отмена",
    "Catalog URL": "URL каталога",
    "Check for game updates and new Arachnel versions.": "Проверка обновлений игр и новых версий Arachnel.",
    "Checks for new versions in the background.": "Проверяет новые версии в фоне.",
    "A quick setup: language, storage, plugins, and a few defaults. Change anything later in Settings.": "Короткая настройка: язык, хранилище, плагины и несколько значений по умолчанию. Всё можно изменить потом в Настройках.",
    "Pick light or dark theme, palette, and accent color. Change later in Settings.": "Светлая или тёмная тема, палитра и акцентный цвет. Потом можно изменить в Настройках.",
    "Choose where games are installed. Downloads go to a subfolder on the same drive.": "Выберите, куда ставить игры. Загрузки попадают в подпапку на том же диске.",
    "Plugins enable automatic install and Play (e.g. FreeTP). Without one, you can still browse catalogs and install manually.": "Плагины включают автоустановку и «Играть» (например FreeTP). Без плагина можно смотреть каталоги и ставить игры вручную.",
    "Official plugins are coming soon. For now, install a plugin file you already have (e.g. FreeTP).": "Официальные плагины скоро появятся. Пока установите файл плагина, если он уже есть (например FreeTP).",
    "Notify you when a newer build is available.": "Сообщать, когда доступна более новая сборка.",
    "Check for new Arachnel versions automatically.": "Автоматически проверять новые версии Arachnel.",
    "Windows games need Proton on Linux. Install it now or later in Settings → Launch.": "Windows-играм на Linux нужен Proton. Установите сейчас или позже в Настройки → Запуск.",
    "Open Catalog to browse games. Change language, storage, and plugins anytime in Settings.": "Откройте Каталог, чтобы выбрать игры. Язык, хранилище и плагины — в Настройках.",
    "Tip: with a plugin installed, Install runs automatically after download. Catalog-only setups need a manual Install step.": "Подсказка: с плагином установка идёт автоматически после загрузки. Только каталог — нужна ручная установка.",
    "Shows when a newer build is available in the catalog.": "Показывает, когда в каталоге есть более новая сборка.",
    "Downloads updates when you open the catalog. You can turn this off per game.": "Скачивает обновления при открытии каталога. Можно отключить для отдельной игры.",
    "Extra options added to every game launch.": "Дополнительные параметры для каждого запуска игры.",
    "Launch options": "Параметры запуска",
    "Can't install %1 — install a plugin for this source": "Нельзя установить %1 — установите плагин для этого источника",
    "Plugin not found for %1 — install it in Settings → Plugins": "Плагин для %1 не найден — установите в Настройки → Плагины",
    "No download link for %1": "Нет ссылки для загрузки: %1",
    "Plugin files (*.arach)": "Файлы плагинов (*.arach)",
    "Invalid plugin file. Choose a plugin package (.arach)": "Неверный файл плагина. Выберите пакет плагина (.arach)",
    "No download link": "Нет ссылки для загрузки",
    "Add a catalog to browse games, or install a plugin for download, install, and play.": "Добавьте каталог, чтобы смотреть игры, или установите плагин для загрузки, установки и запуска.",
    "Paste a catalog link in Settings → Hydra catalogs. Games show up in Catalog.": "Вставьте ссылку на каталог в Настройки → Каталоги Hydra. Игры появятся в Каталоге.",
    "Pick a game in Catalog to start a download.": "Выберите игру в Каталоге, чтобы начать загрузку.",
    "Add a catalog URL from Hydra or another community list. Install a plugin (e.g. FreeTP) to install and play.": "Добавьте URL каталога из Hydra или другого списка. Установите плагин (например FreeTP), чтобы ставить и запускать игры.",
    "Tap Add catalog and paste the catalog link.": "Нажмите «Добавить каталог» и вставьте ссылку.",
    "Paste a catalog URL. Arachnel loads the game list; a plugin handles install and launch.": "Вставьте URL каталога. Arachnel загрузит список игр; плагин отвечает за установку и запуск.",
    "Missing your language? Help translate Arachnel on <a href=\"%1\">Weblate</a>.": "Нет вашего языка? Помогите перевести Arachnel на <a href=\"%1\">Weblate</a>.",
    "Use Install plugin below and pick a plugin file (e.g. FreeTP).": "Нажмите «Установить плагин» ниже и выберите файл плагина (например FreeTP).",
    "Plugins add catalogs and handle install, updates, and launch (e.g. FreeTP).": "Плагины добавляют каталоги и отвечают за установку, обновления и запуск (например FreeTP).",
    "Your library is empty. Install a plugin, pick a game in Catalog, and it will appear here.": "Библиотека пуста. Установите плагин, выберите игру в Каталоге — и она появится здесь.",
    "Install a plugin (e.g. FreeTP) in Settings → Plugins.": "Установите плагин (например FreeTP) в Настройки → Плагины.",
    "Download finished. Click Install to set up the game.": "Загрузка завершена. Нажмите «Установить», чтобы поставить игру.",
    "Browse games from your catalogs and sources.": "Игры из ваших каталогов и источников.",
    "Add a catalog or install a plugin in Settings.": "Добавьте каталог или установите плагин в Настройках.",
    "Turn on one or more sources above — or leave them all off.": "Включите один или несколько источников выше — или оставьте все выключенными.",
    "Libraries on disks — like Steam. You can add other drives.": "Библиотеки на дисках — как в Steam. Можно добавить другие диски.",
    "Delete application data…": "Удалить данные приложения…",
    "Delete application data?": "Удалить данные приложения?",
    "This cannot be undone. Settings, plugins, caches, and library records will be removed. Game files on disk stay in place.": "Это нельзя отменить. Настройки, плагины, кэши и записи библиотеки будут удалены. Файлы игр на диске останутся.",
    "Delete and quit": "Удалить и выйти",
    # Onboarding
    "Welcome to Arachnel": "Добро пожаловать в Arachnel",
    "Step %1 of %2": "Шаг %1 из %2",
    "Skip": "Пропустить",
    "A quick setup before you start": "Короткая настройка перед стартом",
    "We'll pick your language, where games are stored, optional source plugins, and a few defaults. You can change everything later in Settings.": "Выберем язык, папку для игр, при желании плагины источников и несколько настроек по умолчанию. Всё можно изменить потом в Настройках.",
    "Language": "Язык",
    "Choose the interface language.": "Выберите язык интерфейса.",
    "Appearance": "Оформление",
    "Dark or light theme. Accents and palettes are in Settings → Appearance.": "Тёмная или светлая тема. Акценты и палитры — в Настройки → Оформление.",
    "Theme, Material palette, and accent color. You can change these later in Settings.": "Тема, палитра Material и акцентный цвет. Потом можно изменить в Настройках.",
    "Palette": "Палитра",
    "Primary": "Основной",
    "Dark": "Тёмная",
    "Light": "Светлая",
    "Game library folder": "Папка библиотеки игр",
    "Pick the drive or folder where games will be installed. Downloads go into a downloads subfolder. You can add more drives later in Settings → Storage.": "Выберите диск или папку, куда ставить игры. Загрузки попадут в подпапку downloads. Другие диски можно добавить позже в Настройки → Хранилище.",
    "Choose folder…": "Выбрать папку…",
    "Or keep the default path already listed above.": "Или оставьте путь по умолчанию из списка выше.",
    "Source plugins": "Плагины источников",
    "Plugins unlock automatic install and Play for a catalog (for example FreeTP). Without a plugin you can still browse Hydra-compatible JSON feeds and install games manually.": "Плагины включают автоустановку и «Играть» для каталога (например FreeTP). Без плагина можно смотреть JSON-фиды как у Hydra и ставить игры вручную.",
    "Official plugins": "Официальные плагины",
    "Refresh list": "Обновить список",
    "Loading official plugins…": "Загрузка официальных плагинов…",
    "No official plugins available for this platform.": "Для этой платформы нет официальных плагинов.",
    "Or install a plugin file you already have.": "Или установите файл плагина, если он уже есть.",
    "Installed": "Установлен",
    "Installing…": "Установка…",
    "Install": "Установить",
    "Could not load plugin list: %1": "Не удалось загрузить список плагинов: %1",
    "Plugin list is invalid": "Список плагинов повреждён",
    "Plugin not found in the official list": "Плагин не найден в официальном списке",
    "No download link for this plugin": "Нет ссылки для загрузки этого плагина",
    "Download failed: %1": "Ошибка загрузки: %1",
    "Downloaded plugin file is empty": "Скачанный файл плагина пуст",
    "Plugin file checksum mismatch": "Контрольная сумма файла плагина не совпадает",
    "Could not save plugin file": "Не удалось сохранить файл плагина",
    "Plugin installed: %1": "Плагин установлен: %1",
    "A curated list will appear here soon. For now, install a .arach package you already have (e.g. FreeTP).": "Скоро здесь появится список официальных плагинов. Пока установите пакет .arach, если он уже есть (например FreeTP).",
    "Official plugins are coming soon. For now, install a plugin file you already have (e.g. FreeTP).": "Официальные плагины скоро появятся. Пока установите файл плагина, если он уже есть (например FreeTP).",
    "Install .arach…": "Установить .arach…",
    "Skip for now": "Пока пропустить",
    "Updates": "Обновления",
    "Recommended defaults — change anytime in Settings → Updates.": "Рекомендуемые значения — потом можно изменить в Настройки → Обновления.",
    "Check for game updates": "Проверять обновления игр",
    "When the catalog loads, compare build dates with your library.": "При загрузке каталога сравнивать даты сборок с вашей библиотекой.",
    "Check for Arachnel updates": "Проверять обновления Arachnel",
    "Look for new launcher builds on GitHub Releases.": "Искать новые сборки лаунчера на GitHub Releases.",
    "Proton (Linux)": "Proton (Linux)",
    "Most catalog builds are Windows executables. Proton-GE runs them on Linux. You can install it now or later in Settings → Launch.": "Большинство сборок в каталоге — Windows .exe. Proton-GE запускает их на Linux. Можно установить сейчас или позже в Настройки → Запуск.",
    "Proton ready: %1": "Proton готов: %1",
    "Downloading Proton… %1%": "Загрузка Proton… %1%",
    "Proton already installed": "Proton уже установлен",
    "Download Proton-GE %1": "Скачать Proton-GE %1",
    "Download Proton-GE": "Скачать Proton-GE",
    "I'll do this later": "Сделаю позже",
    "You're all set": "Готово",
    "Open Catalog to browse games, or Settings anytime to change language, storage, plugins, and more.": "Откройте Каталог, чтобы выбрать игры, или зайдите в Настройки — язык, хранилище, плагины и другое.",
    "Tip: with a source plugin installed, Install runs automatically after the download. Hydra JSON catalogs use manual install, like Hydra.": "Подсказка: с плагином источника установка идёт автоматически после загрузки. Для JSON-каталогов Hydra — ручная установка, как в Hydra.",
    "Back": "Назад",
    "Get started": "Начать",
    "Next": "Далее",
    "Filters": "Фильтры",
    "Sort & filters": "Сортировка и фильтры",
    "Sort": "Сортировка",
    "Type": "Тип",
    "Size": "Размер",
    "Added": "Добавлено",
    "Extras": "Дополнительно",
    "Genre": "Жанр",
    "Players": "Игроки",
    "Single-player": "Одиночная",
    "Co-op": "Кооп",
    "Multiplayer": "Мультиплеер",
    "All": "Все",
    "Online fix": "Онлайн-фикс",
    "Any": "Любой",
    "< 1 GB": "< 1 ГБ",
    "1–5 GB": "1–5 ГБ",
    "5–20 GB": "5–20 ГБ",
    "20+ GB": "20+ ГБ",
    "Last 7 days": "За 7 дней",
    "Last 30 days": "За 30 дней",
    "Last 90 days": "За 90 дней",
    "Last year": "За год",
    "Has add-ons": "Есть дополнения",
    "Clear all": "Сбросить всё",
    "Apply": "Применить",
    "Any size": "Любой размер",
    "Any time": "Любое время",
    "Largest first": "Сначала большие",
    "Smallest first": "Сначала маленькие",
    "Newest first": "Сначала новые",
    "Oldest first": "Сначала старые",
    "Title A–Z": "Название А–Я",
    "Title Z–A": "Название Я–А",
    "Portable first": "Сначала портабл",
    "Non-portable first": "Сначала не портабл",
    "Filter by type": "Фильтр по типу",
    "Non-portable": "Не портабл",
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
        ru = EN_TO_RU.get(source)

        # Known mapping wins — keep Weblate/manual strings in sync when we rewrite English copy.
        if ru is not None:
            oldsource = message.find("oldsource")
            if oldsource is not None:
                message.remove(oldsource)
            if existing != ru or is_unfinished:
                translation_el.text = ru
                translation_el.attrib.pop("type", None)
                filled += 1
            continue

        if existing.strip() and not is_unfinished:
            continue

        # lupdate may copy a translation but leave type="unfinished" — finalize it.
        if existing.strip() and is_unfinished:
            translation_el.attrib.pop("type", None)
            filled += 1
            continue

        if is_unfinished or not existing.strip():
            missing.append(source)
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

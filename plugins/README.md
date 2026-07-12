# Плагины источников

Каждый каталог — отдельный источник со своим пайплайном установки и обновления.

## Статус

| Плагин | Статус |
|--------|--------|
| `online-fix` | не начат |
| `freetp` | не начат |

**Промежуточно:** источники настраиваются в UI (имя + URL JSON-каталога) и хранятся в `settings.json`. Это не заменяет плагины — только этап до `PluginHost`.

## Планируемые ячейки

| Плагин | Особенности установки |
|--------|------------------------|
| `online-fix` | Portable-архивы, распаковка |
| `freetp` | Installer / portable / встроенный фикс / отдельный патч |

## Контракт

См. [docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md) и [docs/ROADMAP.md](../docs/ROADMAP.md).

Каждый плагин — **самодостаточная ячейка**: свои библиотеки, установка, обновление, `launchInfo`. Ядро оркестрирует библиотеку и torrent-загрузку; распаковку и парсинг сайтов ядро не реализует.

## Целевая структура

```
plugins/
  freetp/
    plugin.json
    CMakeLists.txt
    src/
      freetp_plugin.cpp
      portable_installer.cpp
    README.md
```

`plugin.json` — манифест для `PluginHost` (id, name, version, capabilities, entry point).

Ядро линкуется только с `ISourcePlugin` (`src/core/plugin_interface.h`).

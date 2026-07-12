# Архитектура Arachnel

## Слои

```
┌─────────────────────────────────────────────────────────────┐
│  UI (QML) — библиотека, каталог, загрузки, детали, настройки │
├─────────────────────────────────────────────────────────────┤
│  Core — stores, каталог, torrent-задачи, метаданные, фасад   │
├─────────────────────────────────────────────────────────────┤
│  Source plugins — Online-Fix, FreeTP, … (целевое; TODO)      │
└─────────────────────────────────────────────────────────────┘
```

### Core (ядро)

Ответственность ядра — то, что **одинаково для всех источников**:

| Модуль | Файлы | Назначение |
|--------|-------|------------|
| `CoreController` | `core_controller.*` | Единый QML-фасад (`Arachnel.Core`) |
| `SettingsStore` | `settings_store.*` | Папки, лимит загрузок, список источников |
| `LibraryStore` | `library_store.*` | Установленные игры, компоненты/DLC |
| `JobOrchestrator` | `job_orchestrator.*` | Очередь torrent-задач, прогресс, отмена |
| `TorrentSession` | `torrent_session.*` | libtorrent: magnet → `downloadsRoot` |
| `CatalogFeedLoader` | `catalog_feed_loader.*` | JSON-фид каталога (Hydra/FreeTP) |
| `GameMetadataService` | `game_metadata_service.*` | Обложка/описание через Steam API |
| `CoverImageCache` | `cover_image_cache.*` | Локальный кэш обложек |
| `SourcePluginModel` | `source_plugin_model.*` | **Промежуточно:** источники из настроек (URL JSON) |
| `*_model` | `library_model`, `catalog_model`, `job_model` | Модели для QML |

Ядро **не** знает, как распаковать конкретный архив Online-Fix или когда у FreeTP нужен отдельный фикс.

**Персистентность** (через `QStandardPaths::AppDataLocation`):

- `settings.json` — пути, `sources[]`, лимит параллельных загрузок
- `library.json` — библиотека
- `jobs.json` — активные и завершённые задачи
- кэш обложек — рядом с данными приложения

### Источники: сейчас и цель

**Сейчас (промежуточная модель):** пользователь добавляет источники в настройках — имя + URL JSON-каталога. `SourcePluginModel` хранит список; `CatalogFeedLoader` грузит фид по URL. Встроенных плагинов и `PluginHost` пока нет.

**Цель:** `PluginHost` загружает ячейки из `plugins/<source-id>/`; ядро делегирует install/update/launch через `ISourcePlugin`.

### Source plugin (плагин источника)

Один плагин = один тип источника (или семейство с общим API).

Плагин реализует свой пайплайн:

| Этап | Примеры различий между источниками |
|------|-----------------------------------|
| Каталог | API сайта, парсинг, локальный индекс |
| Метаданные | версия, размер, тип дистрибутива |
| Загрузка | прямая ссылка, magnet, несколько частей |
| Установка | unzip portable, silent installer, копирование + патч |
| Обновление | diff, полная перекачка, замена папки |
| Запуск | `exe` в корне, подпапка `Binaries`, аргументы |

**Торрент** сейчас в ядре (`TorrentSession`), потому что JSON-фиды отдают magnet. После завершения загрузки ядро создаёт запись в библиотеке с `downloadPath`, но **без** `installPath` — установку должен выполнить плагин.

### UI (QML)

| Путь | Содержимое |
|------|------------|
| `qml/app/` | `AppWindow`, `LibraryPage`, `CatalogPage`, `DownloadsPage`, `GameDetailsPage` |
| `qml/settings/` | Bottom sheet: хаб, источники, хранилище, внешний вид, форма источника |
| `qml/components/` | Карточки, rail, кастомный title bar (Windows), пустые состояния |
| `qml/nav/` | `PageNavigator` — стек с fade/bounce |
| `qml/theme/` | Material 3: `Appearance`, `AccentColors` |

UI вызывает `Core` (`Arachnel.Core`); Core оркестрирует stores и torrent. На Windows — frameless window + `AppTitleBar`.

## Контракт плагина источника

Черновик в `src/core/plugin_interface.h`:

```cpp
class ISourcePlugin {
    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QStringList capabilities() const = 0;

    virtual QVector<CatalogEntry> search(...) const = 0;
    virtual std::optional<CatalogEntry> metadata(...) const = 0;

    virtual void install(const InstallContext& ctx) = 0;
    virtual void cancelInstall(const QString& jobId) = 0;

    virtual std::optional<QString> detectUpdate(...) const = 0;
    virtual void update(const InstallContext& ctx, const LibraryGame& local) = 0;

    virtual LaunchInfo launchInfo(const LibraryGame& local) const = 0;
};
```

`InstallContext` передаёт `magnetUri`, `targetPath`, `downloadsPath`, `installKind` — плагин выполняет установку после того, как ядро скачало торрент.

Тип установки (`installKind`) — для UX в UI; **логику выполняет плагин**:

- `portable_archive` — распаковка;
- `installer` — тихая установка;
- `bundled_fix` — готовая сборка;
- `fix_download` — игра + отдельный патч.

## Примеры плагинов (целевые)

### online-fix

- дистрибутив: чаще portable-архив;
- установка: скачать → распаковать в `{library}/{appId}` → записать манифест;
- обновление: сравнить `uploadDate` с каталогом → перекачать/заменить.

### freetp

- дистрибутив: **зависит от страницы игры**;
- каталог: JSON-фид (см. [CATALOG_FORMAT.md](CATALOG_FORMAT.md));
- установка: ветвление внутри плагина (installer / portable / fix overlay);
- обновление: по `uploadDate` и компонентам DLC.

## Сборка и зависимости

| Компонент | Как подключается |
|-----------|------------------|
| Qt 6.8+ | `find_package(Qt6)` — Core, Gui, Network, Qml, Quick, ShaderTools, QuickPrivate |
| QmlMaterial | FetchContent → `qml_modules/Qcm/Material` |
| libtorrent | `cmake/ArachnelLibtorrent.cmake` — FetchContent v2.0.11 + Boost headers |
| Windows | `cmake/patch-qml-material-win.cmake` — SHARED, без spirv-opt |

Опции CMake:

- `ARACHNEL_FAST_BUILD` — `NO_CACHEGEN` для QML (быстрее dev-сборка)
- `ARACHNEL_LIBTORRENT_SHARED` — DLL libtorrent на Windows (быстрее инкрементальные линковки)

Скрипты разработки: `run.sh` (Linux), `run.ps1` (Windows — configure, build, windeployqt, run).

## Структура репозитория

```
src/
  main.cpp
  core/                 # ядро (реализовано)
cmake/
  ArachnelLibtorrent.cmake
  patch-qml-material-win.cmake
qml/
  Main.qml
  app/                  # страницы
  settings/             # настройки (bottom sheet)
  components/
  nav/
  theme/
plugins/                # ячейки источников (пока только README)
docs/
run.sh / run.ps1
```

Пошаговый план: [ROADMAP.md](ROADMAP.md).

Ключевой принцип: **ядро — тонкий оркестратор**; **плагин — самодостаточная ячейка** со своими зависимостями и пайплайном установки.

## Отличие от Hydra в одном предложении

Hydra оптимизирован под **один** способ наполнить библиотеку; Arachnel оптимизирован под **много способов**, каждый из которых инкапсулирован в плагине источника.

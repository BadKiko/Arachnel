# Архитектура Arachnel

Arachnel — Qt/QML launcher с доменным ядром и внешними плагинами источников.
Навигация по доменам: [`src/core/README.md`](../src/core/README.md).

## Карта доменов

```text
QML UI ── Arachnel.Core (CoreController)
                    │ тонкий QML boundary и композиция сервисов
 ┌──────────────────┼─────────────────────────────────────────────────┐
 │ catalog │ library │ jobs │ install │ launch │ runtime │ settings    │
 │ plugins │ torrent │ facade │ i18n │ util                            │
 └──────────────────┴─────────────────────────────────────────────────┘
                    │ ISourcePlugin C ABI
       external .arach / shared-library source plugins
```

`CoreController` публикует стабильную поверхность `Arachnel.Core` для QML и
делегирует работу доменным контроллерам и сервисам. Он не является местом для
новой бизнес-логики конкретного источника.

| Домен | Владеет |
|-------|---------|
| `catalog/` | Фидами и плагинными каталогами, парсингом, фильтрами, metadata и обложками |
| `library/` | `LibraryStore`, модели, storage libraries, обслуживанием и проверкой обновлений |
| `jobs/` | `JobStore`, QML-модель, `JobOrchestrator` и HTTP-загрузками |
| `install/` | Анализом артефактов, install kind и `InstallSessionService` |
| `launch/` | Разрешением launch info, запуском и отслеживанием процесса |
| `runtime/` | Контейнерами runtime, зависимостями, Proton и Windows runner |
| `plugins/` | `PluginHost`, пакетом `.arach`, каталогом плагинов и ABI |
| `torrent/` | Настройками libtorrent, torrent-сессией и metadata fetching |
| `settings/` | Настройками, уведомлениями и обновлением приложения |
| `facade/` | `CoreController`, crash façade и склейкой сервисов |
| `i18n/`, `util/` | Переводами и малыми общими утилитами |

`src/app/` содержит точку входа и crash reporting, а не доменную логику.

## Потоки ответственности

- `CatalogController` объединяет каталоги URL-источников и загруженных плагинов.
- `InstallSessionService` создаёт и завершает install-сессии, передавая
  source-specific работу в плагин.
- `JobOrchestrator` управляет HTTP/torrent задачами; `TorrentSession` — общий
  транспорт для magnet-загрузок.
- `LibraryController`, `LibraryMaintenanceService` и `GameUpdateService`
  изменяют и обслуживают библиотеку.
- `LaunchController` запрашивает у плагина `LaunchInfo`; `ProcessLauncher` и
  `ProcessTracker` исполняют и наблюдают процесс.
- `RuntimeDependencyService` подготавливает runtime-зависимости там, где это
  требуется платформой.

Персистентные stores используют `QStandardPaths::AppDataLocation`: настройки,
библиотека, jobs и кэш обложек находятся в данных приложения.

## Плагины источников

`PluginHost` сканирует bundle и пользовательские каталоги плагинов, загружает
shared library из пакета и предоставляет её `ISourcePlugin` доменам core.
Плагины живут в отдельных репозиториях; см. [`plugins/README.md`](../plugins/README.md).

Контракт: `src/core/plugins/plugin_interface.h`; C ABI:
`src/core/plugins/plugin_api.h`. Текущая версия ABI — **v3**. Хост принимает
плагины API **v2–v3**; API v3 добавляет capability `owns_download`.

Плагин владеет source-specific поведением: каталогом, анализом дистрибутива,
установкой, обновлениями и `LaunchInfo`. Для обычного magnet-потока core
скачивает артефакт, после чего плагин получает `InstallContext`. При
`owns_download` плагин сам ведёт download/install и сообщает прогресс хосту.
В core нельзя добавлять ветвления вида `if (sourceId == ...)`.

## UI (QML)

| Путь | Содержимое |
|------|------------|
| `qml/app/` | `AppWindow`, `LibraryPage`, `CatalogPage`, `DownloadsPage`, `GameDetailsPage` |
| `qml/settings/` | Bottom sheet: хаб, источники, хранилище, внешний вид, форма источника |
| `qml/components/` | Карточки, rail, кастомный title bar (Windows), пустые состояния |
| `qml/nav/` | `PageNavigator` — стек с fade/bounce |
| `qml/theme/` | Material 3: `Appearance`, `AccentColors` |

UI вызывает `Core` (`Arachnel.Core`) и получает модели библиотеки, каталога,
jobs, sources, notifications и settings. На Windows используется frameless
window с `AppTitleBar`.

## Сборка

- Root [`CMakeLists.txt`](../CMakeLists.txt) + [`src/CMakeLists.txt`](../src/CMakeLists.txt) (список исходников и include dirs).
- Плагины: [`cmake/ArachnelPluginSdk.cmake`](../cmake/ArachnelPluginSdk.cmake).

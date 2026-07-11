# Arachnel

Лаунчер игр с упором на **online-fix** и **плагинную систему источников**.

По духу близок к Hydra Launcher, но вместо одной универсальной схемы загрузки/установки — **отдельный плагин на каждый тип источника** (Online-Fix, FreeTP, …), потому что у них разные форматы: portable, installer, встроенный фикс, отдельный патч.

**Видение:** [docs/VISION.md](docs/VISION.md)  
**Архитектура:** [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)  
**Roadmap:** [docs/ROADMAP.md](docs/ROADMAP.md)  
**Формат каталога:** [docs/CATALOG_FORMAT.md](docs/CATALOG_FORMAT.md)

## Что уже работает

- UI: библиотека, каталог (несколько JSON-источников), загрузки, детали игры, настройки
- Torrent-загрузка по magnet из каталога (libtorrent)
- Персистентность: настройки, библиотека, задачи
- Обложки и описания через Steam API
- Проверка обновлений по `uploadDate`

**Пока нет:** установка после загрузки, запуск игры, плагины в `plugins/`.

## Зависимости

- Qt 6.8+ (Core, Gui, Network, Qml, Quick, ShaderTools)
- CMake 3.20+, Ninja
- libtorrent-rasterbar (системный пакет или сборка из исходников — см. `cmake/ArachnelLibtorrent.cmake`)
- git-lfs (шрифты QmlMaterial)

### Linux

```bash
sudo apt install build-essential cmake pkg-config git-lfs \
  libtorrent-rasterbar-dev qt6-base-dev qt6-declarative-dev qt6-tools-dev
```

### Windows

Qt 6.x с MinGW (`D:\Qt\6.11.1\mingw_64` или аналог), CMake, Ninja. libtorrent и Boost headers подтягиваются при configure.

## Запуск

```bash
# Linux
./run.sh              # configure + build + run
./run.sh --rebuild    # чистая сборка
./run.sh --run        # только запуск
```

```powershell
# Windows
.\run.ps1              # configure + build + run
.\run.ps1 --rebuild    # чистая build-win/
.\run.ps1 --run        # только запуск
```

Переменные окружения:

| Переменная | Назначение |
|------------|------------|
| `BUILD_TYPE` | `RelWithDebInfo` (Win по умолчанию), `Debug` |
| `CMAKE_PREFIX_PATH` | Путь к Qt kit |
| `ARACHNEL_FAST_BUILD=0` | Включить QML cachegen |
| `ARACHNEL_LIBTORRENT_SHARED=0` | Статическая libtorrent (Win) |

## Структура

```
src/core/            C++ ядро (stores, torrent, каталог, фасад Core)
qml/
  Main.qml           точка входа
  app/               LibraryPage, CatalogPage, DownloadsPage, …
  settings/          настройки (bottom sheet)
  components/        карточки, title bar, rail
  nav/               PageNavigator
  theme/             Material 3
cmake/               libtorrent, патч QmlMaterial
plugins/             плагины источников (целевое; пока пусто)
docs/                видение, архитектура, roadmap
run.sh / run.ps1     dev-лаунчеры
```

### Куда добавлять новое

| Задача | Куда |
|--------|------|
| Новая страница | `qml/app/<Name>Page.qml` → `AppWindow.qml` |
| Настройки | `qml/settings/` |
| Логика ядра | `src/core/` |
| Плагин источника | `plugins/<source-id>/` |
| C++ в QML | `main.cpp` + `core_controller` |

## Сборка вручную

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target arachnel_app
./build/arachnel_app
```

На Windows каталог сборки по умолчанию — `build-win/` (см. `run.ps1`).

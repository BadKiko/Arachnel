# Arachnel

Лаунчер игр с упором на **online-fix** и **плагинную систему источников**.

По духу близок к Hydra Launcher, но вместо одной универсальной схемы загрузки/установки — **отдельный плагин на каждый тип источника** (Online-Fix, FreeTP, …), потому что у них разные форматы: portable, installer, встроенный фикс, отдельный патч.

**Видение и отличия:** [docs/VISION.md](docs/VISION.md)  
**Архитектура и контракт плагинов:** [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)  
**Карта движения (roadmap):** [docs/ROADMAP.md](docs/ROADMAP.md)  
**Формат каталога (JSON/magnet):** [docs/CATALOG_FORMAT.md](docs/CATALOG_FORMAT.md)

## Зависимости сборки

- Qt 6.8+ (Core, Gui, Network, Qml, Quick)
- libtorrent-rasterbar (`libtorrent-rasterbar-dev`)
- g++, cmake, pkg-config

```bash
sudo apt install build-essential cmake pkg-config git-lfs \
  libtorrent-rasterbar-dev qt6-base-dev qt6-declarative-dev qt6-tools-dev
```

`git-lfs` нужен для иконок Material (QmlMaterial хранит шрифты через LFS). `run.sh` подтянет их автоматически.

## Запуск

```bash
./run.sh --rebuild
```

## Структура

```
src/                 C++ entry (далее — core, plugin host)
qml/
  Main.qml           точка входа QML
  app/               оболочка и страницы (библиотека, каталог, …)
  settings/          настройки
  theme/             тема Material 3
plugins/             плагины источников (online-fix, freetp, …)
docs/                видение и архитектура
```

### Куда добавлять новое

| Задача | Куда |
|--------|------|
| Новая страница UI | `qml/app/<Name>Page.qml` → `AppWindow.qml` |
| Настройки | `qml/settings/` |
| Тема | `qml/theme/` |
| Логика ядра | `src/core/` |
| Плагин источника | `plugins/<source-id>/` |
| C++ backend | `src/` + регистрация в `main.cpp` |

## Сборка

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/arachnel_app
```

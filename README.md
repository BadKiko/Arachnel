# Arachnel

Лаунчер игр с упором на **online-fix** и **плагинную систему источников**.

По духу близок к Hydra Launcher, но вместо одной универсальной схемы загрузки/установки — **отдельный плагин на каждый тип источника** (Online-Fix, FreeTP, …), потому что у них разные форматы: portable, installer, встроенный фикс, отдельный патч.

**Видение и отличия:** [docs/VISION.md](docs/VISION.md)  
**Архитектура и контракт плагинов:** [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)

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
| Логика ядра | `src/core/` (планируется) |
| Плагин источника | `plugins/<source-id>/` |
| C++ backend | `src/` + регистрация в `main.cpp` |

## Сборка

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/arachnel_app
```

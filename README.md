# Arachnel

Qt 6 + QmlMaterial (Material 3).

## Запуск

```bash
./run.sh --rebuild
```

## Структура

```
src/                 C++ entry point
qml/
  Main.qml           точка входа QML
  app/               оболочка приложения и страницы
  settings/          bottom sheet и экран настроек
  theme/             singleton'ы темы и константы цветов
```

### Куда добавлять новое

| Задача | Куда |
|--------|------|
| Новая страница | `qml/app/<Name>Page.qml`, подключить в `AppWindow.qml` |
| Настройки / секция | `qml/settings/` |
| Тема, палитра, токены | `qml/theme/Appearance.qml`, `AccentColors.qml` |
| C++ backend | `src/` + регистрация в `main.cpp` |

## Сборка

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/arachnel_app
```

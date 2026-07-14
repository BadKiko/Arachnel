<div align="center">

<img src="resources/icons/arachnel-github.svg" width="128" alt="Логотип Arachnel" />

<h1>Arachnel</h1>

<p>
  <a href="README.md"><img src="https://img.shields.io/badge/README-English-8E8E93?style=for-the-badge&labelColor=161618" alt="English README"></a>
  <a href="README.ru.md"><img src="https://img.shields.io/badge/README-Русский-8E8E93?style=for-the-badge&labelColor=161618" alt="Русский README"></a>
</p>

<p>
  <a href="https://hosted.weblate.org/engage/arachnel/">
    <img src="https://hosted.weblate.org/widget/arachnel/application/ru/svg-badge.svg" alt="Состояние перевода">
  </a>
</p>

<br>

<img src="docs/readme-carousel.svg" width="960" alt="Скриншоты Arachnel">
</div>

<br>

## О проекте

**Arachnel** — десктопный лаунчер игр в духе Hydra, но с **плагинной моделью источников** вместо одного универсального пайплайна установки.

Каждый источник (FreeTP, Online-Fix, …) задаёт свой каталог, загрузку, установку и запуск: portable, installer, фикс в сборке или отдельный патч.

**Уже работает**

- Material 3 UI: библиотека, каталог из нескольких источников, загрузки, детали игры, настройки
- Torrent-загрузка через libtorrent (magnet из JSON каталога)
- Сохранение настроек, библиотеки и задач загрузки
- Обложки и описания через Steam API
- Переводы сообщества на [Weblate](https://hosted.weblate.org/projects/arachnel/)

**Документация:** [Vision](docs/VISION.md) · [Architecture](docs/ARCHITECTURE.md) · [Roadmap](docs/ROADMAP.md) · [Plugin SDK](docs/PLUGIN_SDK.md) · [Translating](docs/TRANSLATING.md)

## Быстрый старт

```bash
# Linux
./run.sh

# Windows
.\run.ps1
```

Сборка вручную, зависимости и переменные окружения — в скриптах репозитория и `cmake/`.

**Разработчикам плагинов:** [docs/PLUGIN_SDK.md](docs/PLUGIN_SDK.md) и [plugins/README.md](plugins/README.md). Плагины — отдельные репозитории (например [arachnel-plugin-freetp](https://github.com/PetWork/arachnel-plugin-freetp)); папки `build-*` в git не коммитятся.

## Связь

**kirill.kif234@gmail.com** — только по важным вопросам.

Всё остальное (баги, идеи, вопросы): [GitHub Issues](https://github.com/BadKiko/Arachnel/issues).





# Arachnel





  






  


## О проекте

**Arachnel** — десктопный лаунчер игр в духе Hydra, но с **плагинной моделью источников** вместо одного универсального пайплайна установки.

Каждый источник (FreeTP, Online-Fix, …) задаёт свой каталог, загрузку, установку и запуск: portable, installer, фикс в сборке или отдельный патч.

**Уже работает**

- Material 3 UI: библиотека, каталог из нескольких источников, загрузки, детали игры, настройки
- Torrent-загрузка через libtorrent (magnet из JSON каталога)
- Сохранение настроек, библиотеки и задач загрузки
- Обложки и описания через Steam API
- Переводы сообщества на [Weblate](https://hosted.weblate.org/projects/arachnel/)

**Документация:** [Vision](docs/VISION.md) · [Architecture](docs/ARCHITECTURE.md) · [Roadmap](docs/ROADMAP.md) · [Translating](docs/TRANSLATING.md)

## Быстрый старт

```bash
# Linux
./run.sh

# Windows
.\run.ps1
```

Сборка вручную, зависимости и переменные окружения — в скриптах репозитория и `cmake/`.

## Связь

**[kirill.kif234@gmail.com](mailto:kirill.kif234@gmail.com)** — только по важным вопросам.

Всё остальное (баги, идеи, вопросы): [GitHub Issues](https://github.com/BadKiko/Arachnel/issues).
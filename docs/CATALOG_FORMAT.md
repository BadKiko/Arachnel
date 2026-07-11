# Формат каталога Arachnel

Каталог источника — JSON-фид, совместимый с [freetp-hydra-link](https://gitlab.com/BadKiko/freetp-hydra-link) и расширенный под Arachnel.

## Базовый формат (Hydra / FreeTP)

```json
{
  "name": "FreeTP",
  "downloads": [
    {
      "title": "Elden Ring",
      "uris": ["magnet:?xt=urn:btih:..."],
      "fileSize": "60 GB",
      "uploadDate": "2026-07-05T11:05:54"
    }
  ]
}
```

| Поле | Назначение |
|------|------------|
| `title` | Название игры (для поиска и скрапинга метаданных Steam) |
| `uris` | Magnet-ссылки торрента (первая `magnet:` используется для загрузки) |
| `fileSize` | Отображаемый размер |
| `uploadDate` | ISO-дата заливки — **маркер версии** для проверки обновлений |

URL по умолчанию для FreeTP в Arachnel:

```
https://gitlab.com/BadKiko/freetp-hydra-link/-/raw/main/games.json?ref_type=heads
```

## Расширения Arachnel

### Явный id и kind

```json
{
  "title": "Elden Ring",
  "id": "freetp-elden-ring",
  "kind": "game",
  "uris": ["magnet:..."],
  "fileSize": "60 GB",
  "uploadDate": "2026-07-05T11:05:54"
}
```

`kind`: `game` | `dlc` | `addon`

### DLC внутри записи игры

```json
{
  "title": "Elden Ring",
  "uris": ["magnet:..."],
  "fileSize": "60 GB",
  "uploadDate": "2026-07-05T11:05:54",
  "addons": [
    {
      "title": "Shadow of the Erdtree",
      "kind": "dlc",
      "uris": ["magnet:..."],
      "fileSize": "8 GB",
      "uploadDate": "2026-06-01T12:00:00"
    }
  ]
}
```

### DLC отдельной строкой (привязка по parentTitle)

```json
{
  "title": "Shadow of the Erdtree",
  "kind": "dlc",
  "parentTitle": "Elden Ring",
  "uris": ["magnet:..."],
  "fileSize": "8 GB",
  "uploadDate": "2026-06-01T12:00:00"
}
```

Ядро сгруппирует такие записи в `addons` родительской игры.

## Обновления

- Сравнение `uploadDate` установленной игры с каталогом.
- Если дата в каталоге новее — `hasUpdate = true`.
- Для DLC — отдельно по каждому компоненту в `components[]` библиотеки.

## Метаданные (обложка, описание)

Ядро обогащает записи через Steam Store Search API по `title`:

- обложка: Steam CDN `library_600x900`
- описание и жанры: `appdetails`
- локальный кэш: `CoverImageCache`

Плагин источника может переопределить или дополнить метаданные.

## Источники в настройках (промежуточная модель)

До появления `PluginHost` пользователь добавляет источники в UI. В `settings.json`:

```json
{
  "libraryRoot": "~/Games/Arachnel",
  "downloadsRoot": "~/Downloads/Arachnel",
  "maxConcurrentDownloads": 2,
  "sources": [
    {
      "id": "freetp",
      "name": "FreeTP",
      "description": "Торрент-каталог FreeTP",
      "catalogUrl": "https://gitlab.com/BadKiko/freetp-hydra-link/-/raw/main/games.json?ref_type=heads",
      "iconName": "storefront",
      "enabled": true,
      "capabilities": ["search", "install", "update"]
    }
  ]
}
```

| Поле | Назначение |
|------|------------|
| `id` | Уникальный slug (генерируется из имени при добавлении) |
| `catalogUrl` | URL JSON-фида — загружается `CatalogFeedLoader` |
| `enabled` | Показывать чип в каталоге |

Миграция: старое поле `freetpCatalogUrl` автоматически превращается в один источник `freetp`.

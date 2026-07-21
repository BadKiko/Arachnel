# Arachnel — roadmap

Этот документ отражает текущее состояние кода. Для границ системы см.
[ARCHITECTURE.md](ARCHITECTURE.md), а для формата JSON-фида —
[CATALOG_FORMAT.md](CATALOG_FORMAT.md).

## Прогресс (обновлено 2026-07-21)

### Сделано

| Область | Статус |
|---------|--------|
| Доменная структура core | ✅ `src/core/<domain>/`, фасад и сервисы разделены |
| QML boundary | ✅ `CoreController` публикует `Arachnel.Core` и делегирует доменам |
| Каталог | ✅ JSON-фиды и plugin catalogs, парсинг, фильтры, metadata, кэш обложек |
| Библиотека | ✅ persistence, storage roots, maintenance и update checks |
| Jobs и транспорт | ✅ `JobOrchestrator`, HTTP, libtorrent и восстановление задач |
| Установка | ✅ `InstallSessionService`, анализ артефактов, plugin install и add-ons |
| Plugin runtime | ✅ `PluginHost`, `.arach` install/uninstall, plugin catalog, ABI v2–v3 |
| Запуск | ✅ plugin `LaunchInfo`, launch resolver, launcher и process tracker |
| Runtime | ✅ Proton management, dependency service, runtime containers и Windows runner |
| Settings | ✅ persistence, notifications и app updater |

### Как работает установка сегодня

1. `CatalogController` получает запись из URL-источника или загруженного плагина.
2. `InstallSessionService` создаёт job через `JobOrchestrator`.
3. Для magnet core использует `TorrentSession`; HTTP-загрузки идут через
   `HttpDownloadSession`.
4. После завершения `InstallSessionService` передаёт `InstallContext` в
   `ISourcePlugin::installFromDownload`.
5. Плагин возвращает install path, а core обновляет библиотеку. Для API v3
   capability `owns_download` плагин сам выполняет download/install и сообщает
   прогресс хосту.

### Что ещё предстоит

Это не список уже отсутствующей инфраструктуры: `PluginHost`, install и launch
уже существуют. Следующие задачи — развитие и проверка поведения:

| Направление | Следующий результат |
|-------------|--------------------|
| Источники | Плагин `online-fix` и расширение совместимых source plugins |
| Совместимость | Тестирование ABI v2/v3 и packaging на Windows/Linux |
| Install/update | Больше реальных сценариев add-on, manual install и recovery |
| Runtime | Улучшение диагностики зависимостей и platform-specific UX |
| Надёжность | Тесты persistence, восстановления jobs и ошибок сети |
| UX | Полировка статусов, уведомлений и ошибок plugin/runtime |

## Границы, которые сохраняем

- Источник владеет своим каталогом, установкой, обновлением и launch hints.
- Core владеет общими models, stores, очередью задач, транспортом и
  оркестрацией.
- `CoreController` остаётся QML boundary, а не новым доменом бизнес-логики.
- Новый код размещается в соответствующем `src/core/<domain>/`; ориентир по
  размеру `.cpp` — не более 400 строк.

Подробности о добавлении сервисов: [CONTRIBUTING.md](CONTRIBUTING.md).

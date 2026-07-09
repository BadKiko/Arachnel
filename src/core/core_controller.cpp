#include "core_controller.h"

#include "catalog_feed_loader.h"
#include "game_metadata_service.h"
#include "job_orchestrator.h"
#include "library_store.h"
#include "settings_store.h"
#include "torrent_session.h"

#include <QDateTime>
#include <QJSEngine>
#include <QQmlEngine>
#include <QtQml/qqml.h>

namespace arachnel::core {

CoreController* CoreController::create(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    return &instance();
}

CoreController& CoreController::instance()
{
    static CoreController controller;
    return controller;
}

CoreController::CoreController(QObject* parent)
    : QObject(parent)
{
    initializeServices();
    loadSources();
    m_settings.load();
    m_libraryStore.load();
    syncLibraryFromStore();
}

void CoreController::initializeServices()
{
    m_catalogLoader = new CatalogFeedLoader(this);
    m_metadataService = new GameMetadataService(this);
    m_torrentSession = new TorrentSession(this);
    m_jobOrchestrator =
        new JobOrchestrator(&m_settings, m_torrentSession, &m_jobs, this);

    connect(m_catalogLoader, &CatalogFeedLoader::feedLoaded, this,
            [this](const QString& sourceId, const QVector<CatalogEntry> entries) {
                if (sourceId != m_activeSourceId)
                    return;

                m_catalogCache = entries;
                setCatalogLoading(false);
                setCatalogStatus(
                    QStringLiteral("Загружено %1 игр, обогащение метаданных…").arg(entries.size()));
                applyCatalogFilter(sourceId, m_activeQuery);

                auto* mutableCache = &m_catalogCache;
                m_metadataService->enrichEntries(*mutableCache);
            });

    connect(m_catalogLoader, &CatalogFeedLoader::feedFailed, this,
            [this](const QString& sourceId, const QString& error) {
                if (sourceId != m_activeSourceId)
                    return;
                setCatalogLoading(false);
                setCatalogStatus(error);
                setLastAction(QStringLiteral("Ошибка каталога: %1").arg(error));
            });

    connect(m_metadataService, &GameMetadataService::entryEnriched, this,
            [this](const QString& entryId) {
                for (auto& entry : m_catalogCache) {
                    if (entry.id != entryId)
                        continue;
                    const GameMetadata metadata = m_metadataService->metadataForTitle(entry.title);
                    if (!metadata.coverUrl.isEmpty())
                        entry.coverUrl = metadata.coverUrl;
                    if (!metadata.description.isEmpty())
                        entry.description = metadata.description;
                    if (!metadata.genres.isEmpty())
                        entry.genres = metadata.genres;
                    entry.metadataPending = false;
                    break;
                }
                applyCatalogFilter(m_activeSourceId, m_activeQuery);
            });

    connect(m_metadataService, &GameMetadataService::enrichmentFinished, this, [this]() {
        setCatalogStatus(QStringLiteral("Каталог готов · %1 игр").arg(m_catalog.rowCount()));
    });

    connect(m_jobOrchestrator, &JobOrchestrator::downloadCompleted, this,
            [this](const QString& jobId, const QString& entryId, const QString& sourceId,
                   const QString& savePath, JobKind kind) {
                Q_UNUSED(jobId)

                const CatalogEntry* entry = findCatalogEntry(entryId);
                if (!entry)
                    return;

                LibraryGame game;
                game.id = entryId;
                game.title = entry->title;
                game.coverUrl = entry->coverUrl;
                game.sourceId = sourceId;
                game.sourceName = m_sources.nameForId(sourceId);
                game.version = entry->version;
                game.installPath = QString();
                game.description = entry->description;
                game.genres = entry->genres;
                game.sizeLabel = entry->sizeLabel;
                game.installKind = entry->installKind;
                game.uploadDate = entry->uploadDate;
                game.magnetUri = entry->magnetUris.value(0);
                game.downloadPath = savePath;
                game.hasUpdate = false;

                for (const auto& addon : entry->addons) {
                    InstalledComponent component;
                    component.id = addon.id;
                    component.title = addon.title;
                    component.uploadDate = addon.uploadDate;
                    component.installed = false;
                    game.components.append(component);
                }

                m_libraryStore.upsertGame(game);
                syncLibraryFromStore();

                if (kind == JobKind::Update)
                    setLastAction(QStringLiteral("Обновление загружено: %1").arg(entry->title));
                else
                    setLastAction(QStringLiteral("Загрузка завершена: %1").arg(entry->title));
            });

    connect(m_jobOrchestrator, &JobOrchestrator::downloadFailed, this,
            [this](const QString& jobId, const QString& error) {
                Q_UNUSED(jobId)
                setLastAction(QStringLiteral("Ошибка загрузки: %1").arg(error));
            });
}

void CoreController::loadSources()
{
    m_sources.setPlugins({
        {QStringLiteral("freetp"),
         QStringLiteral("FreeTP"),
         QStringLiteral("Торрент-каталог FreeTP, magnet-ссылки, дополнения"),
         {QStringLiteral("search"), QStringLiteral("install"), QStringLiteral("update")}},
        {QStringLiteral("online-fix"),
         QStringLiteral("Online-Fix"),
         QStringLiteral("Portable-архивы (плагин в разработке)"),
         {QStringLiteral("search"), QStringLiteral("install"), QStringLiteral("update")}},
    });
}

void CoreController::syncLibraryFromStore()
{
    m_library.setGames(m_libraryStore.games());
}

void CoreController::setLastAction(const QString& action)
{
    if (m_lastAction == action)
        return;
    m_lastAction = action;
    emit lastActionChanged();
}

void CoreController::setCatalogLoading(bool loading)
{
    if (m_catalogLoading == loading)
        return;
    m_catalogLoading = loading;
    emit catalogLoadingChanged();
}

void CoreController::setCatalogStatus(const QString& status)
{
    if (m_catalogStatus == status)
        return;
    m_catalogStatus = status;
    emit catalogStatusChanged();
}

bool CoreController::isRemoteUploadDateNewer(const QString& remote, const QString& local) const
{
    if (remote.isEmpty() || local.isEmpty())
        return false;
    const QDateTime remoteDate = QDateTime::fromString(remote, Qt::ISODate);
    const QDateTime localDate = QDateTime::fromString(local, Qt::ISODate);
    if (!remoteDate.isValid() || !localDate.isValid())
        return remote > local;
    return remoteDate > localDate;
}

const CatalogEntry* CoreController::findCatalogEntry(const QString& entryId) const
{
    for (const auto& entry : m_catalogCache) {
        if (entry.id == entryId)
            return &entry;
    }
    return m_catalog.entryById(entryId);
}

const CatalogComponent* CoreController::findCatalogAddon(const CatalogEntry& entry,
                                                         const QString& addonId) const
{
    for (const auto& addon : entry.addons) {
        if (addon.id == addonId)
            return &addon;
    }
    return nullptr;
}

void CoreController::applyCatalogFilter(const QString& sourceId, const QString& query)
{
    const QString needle = query.trimmed().toLower();
    QVector<CatalogEntry> filtered;
    filtered.reserve(m_catalogCache.size());

    for (const auto& entry : m_catalogCache) {
        if (entry.sourceId != sourceId)
            continue;
        if (!needle.isEmpty() && !entry.title.toLower().contains(needle))
            continue;
        filtered.append(entry);
    }

    m_catalog.setEntries(std::move(filtered));
    setLastAction(QStringLiteral("Каталог %1: найдено %2").arg(sourceId).arg(m_catalog.rowCount()));
}

void CoreController::launchGame(const QString& gameId)
{
    const LibraryGame* game = m_library.gameById(gameId);
    if (!game) {
        setLastAction(QStringLiteral("Игра не найдена: %1").arg(gameId));
        return;
    }
    if (game->installPath.isEmpty()) {
        setLastAction(QStringLiteral("%1 загружена, установка плагином ещё не реализована")
                          .arg(game->title));
        return;
    }
    setLastAction(QStringLiteral("Запуск (скоро): %1").arg(game->title));
}

void CoreController::refreshCatalog(const QString& sourceId)
{
    const QString url = m_settings.catalogUrlForSource(sourceId);
    if (url.isEmpty()) {
        setCatalogStatus(QStringLiteral("Для источника %1 не задан URL каталога").arg(sourceId));
        return;
    }

    m_activeSourceId = sourceId;
    setCatalogLoading(true);
    setCatalogStatus(QStringLiteral("Загрузка каталога…"));
    m_catalogLoader->loadFeed(QUrl(url), sourceId);
}

void CoreController::searchCatalog(const QString& sourceId, const QString& query)
{
    if (!m_sources.pluginById(sourceId)) {
        m_catalog.clear();
        setLastAction(QStringLiteral("Неизвестный источник: %1").arg(sourceId));
        return;
    }

    m_activeSourceId = sourceId;
    m_activeQuery = query;

    if (m_catalogCache.isEmpty() || m_catalogCache.first().sourceId != sourceId) {
        refreshCatalog(sourceId);
        return;
    }

    applyCatalogFilter(sourceId, query);
}

void CoreController::installCatalogEntry(const QString& entryId)
{
    const CatalogEntry* entry = findCatalogEntry(entryId);
    if (!entry) {
        setLastAction(QStringLiteral("Запись каталога не найдена: %1").arg(entryId));
        return;
    }

    if (entry->magnetUris.isEmpty()) {
        setLastAction(QStringLiteral("Нет magnet-ссылки для %1").arg(entry->title));
        return;
    }

    const QString jobId = m_jobOrchestrator->startCatalogDownload(*entry, JobKind::Download);
    if (jobId.isEmpty()) {
        setLastAction(QStringLiteral("Не удалось начать загрузку %1").arg(entry->title));
        return;
    }

    setLastAction(QStringLiteral("Торрент-загрузка: %1").arg(entry->title));
}

void CoreController::installCatalogAddon(const QString& entryId, const QString& addonId)
{
    const CatalogEntry* entry = findCatalogEntry(entryId);
    if (!entry) {
        setLastAction(QStringLiteral("Игра не найдена: %1").arg(entryId));
        return;
    }

    const CatalogComponent* addon = findCatalogAddon(*entry, addonId);
    if (!addon) {
        setLastAction(QStringLiteral("Дополнение не найдено"));
        return;
    }

    const QString jobId = m_jobOrchestrator->startAddonDownload(*entry, *addon);
    if (jobId.isEmpty()) {
        setLastAction(QStringLiteral("Не удалось начать загрузку дополнения"));
        return;
    }

    setLastAction(QStringLiteral("Загрузка дополнения: %1").arg(addon->title));
}

void CoreController::updateCatalogEntry(const QString& entryId)
{
    const CatalogEntry* entry = findCatalogEntry(entryId);
    if (!entry) {
        setLastAction(QStringLiteral("Запись не найдена: %1").arg(entryId));
        return;
    }

    const QString jobId = m_jobOrchestrator->startCatalogDownload(*entry, JobKind::Update);
    if (jobId.isEmpty()) {
        setLastAction(QStringLiteral("Не удалось начать обновление %1").arg(entry->title));
        return;
    }

    setLastAction(QStringLiteral("Обновление: %1").arg(entry->title));
}

void CoreController::checkUpdates()
{
    if (m_catalogCache.isEmpty()) {
        refreshCatalog(QStringLiteral("freetp"));
        setLastAction(QStringLiteral("Сначала загружаем каталог для проверки обновлений…"));
        return;
    }

    QHash<QString, CatalogEntry> remoteById;
    for (const auto& entry : m_catalogCache)
        remoteById.insert(entry.id, entry);

    QVector<LibraryGame> games = m_libraryStore.games();
    int updates = 0;
    for (auto& game : games) {
        const CatalogEntry remote = remoteById.value(game.id);
        bool hasUpdate = false;

        if (!remote.id.isEmpty()
            && isRemoteUploadDateNewer(remote.uploadDate, game.uploadDate)) {
            hasUpdate = true;
        }

        for (auto& component : game.components) {
            for (const auto& remoteAddon : remote.addons) {
                if (remoteAddon.id != component.id)
                    continue;
                if (!component.installed
                    || isRemoteUploadDateNewer(remoteAddon.uploadDate, component.uploadDate)) {
                    hasUpdate = true;
                }
            }
        }

        game.hasUpdate = hasUpdate;
        if (hasUpdate)
            ++updates;
    }

    m_libraryStore.setGames(games);
    syncLibraryFromStore();
    setLastAction(QStringLiteral("Проверка обновлений: найдено %1").arg(updates));
}

void CoreController::cancelJob(const QString& jobId)
{
    m_jobOrchestrator->cancelJob(jobId);
    setLastAction(QStringLiteral("Задача отменена"));
}

void registerCoreTypes()
{
    qmlRegisterSingletonType<CoreController>("Arachnel.Core", 1, 0, "Core", &CoreController::create);
    qmlRegisterUncreatableType<LibraryModel>("Arachnel.Core", 1, 0, "LibraryModel",
                                             QStringLiteral("Use Core.library"));
    qmlRegisterUncreatableType<SourcePluginModel>("Arachnel.Core", 1, 0, "SourcePluginModel",
                                                  QStringLiteral("Use Core.sources"));
    qmlRegisterUncreatableType<CatalogModel>("Arachnel.Core", 1, 0, "CatalogModel",
                                             QStringLiteral("Use Core.catalog"));
    qmlRegisterUncreatableType<JobModel>("Arachnel.Core", 1, 0, "JobModel",
                                         QStringLiteral("Use Core.jobs"));
    qmlRegisterUncreatableType<SettingsStore>("Arachnel.Core", 1, 0, "SettingsStore",
                                              QStringLiteral("Use Core.settings"));
}

} // namespace arachnel::core

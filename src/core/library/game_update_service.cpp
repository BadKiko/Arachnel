#include "game_update_service.h"

#include "job_orchestrator.h"
#include "library_store.h"
#include "plugin_host.h"
#include "plugin_interface.h"
#include "settings_store.h"

#include <QDate>
#include <QDateTime>
#include <QFileInfo>

namespace arachnel::core {

GameUpdateService::GameUpdateService(LibraryStore* store, SettingsStore* settings,
                                     PluginHost* plugins, JobOrchestrator* jobs,
                                     const QVector<CatalogEntry>* catalog, Hooks hooks)
    : m_store(store), m_settings(settings), m_plugins(plugins), m_jobs(jobs), m_catalog(catalog),
      m_hooks(std::move(hooks))
{}

bool GameUpdateService::isRemoteUploadDateNewer(const QString& remote, const QString& local) const
{
    if (remote.isEmpty() || local.isEmpty() || remote == local)
        return false;
    const QDateTime remoteDate = QDateTime::fromString(remote, Qt::ISODate);
    const QDateTime localDate = QDateTime::fromString(local, Qt::ISODate);
    if (remoteDate.isValid() && localDate.isValid())
        return remoteDate > localDate;
    return remote > local;
}

bool GameUpdateService::gameHasUpdate(const LibraryGame& game, const CatalogEntry& remote) const
{
    if (remote.id.isEmpty())
        return false;
    if (ISourcePlugin* plugin = m_plugins ? m_plugins->plugin(game.sourceId) : nullptr) {
        if (plugin->detectUpdate(game, remote))
            return true;
    }
    if (isRemoteUploadDateNewer(remote.uploadDate, game.uploadDate))
        return true;
    for (const InstalledComponent& component : game.components) {
        if (!component.installed)
            continue;
        for (const CatalogComponent& addon : remote.addons) {
            if (addon.id == component.id
                && isRemoteUploadDateNewer(addon.uploadDate, component.uploadDate))
                return true;
        }
    }
    return false;
}

int GameUpdateService::recalculateLibraryUpdates(bool notify)
{
    if (!m_catalog || m_catalog->isEmpty())
        return 0;
    QHash<QString, CatalogEntry> remoteById;
    for (const CatalogEntry& entry : *m_catalog)
        remoteById.insert(entry.id, entry);
    QVector<LibraryGame> games = m_store->games();
    int updates = 0;
    for (LibraryGame& game : games) {
        game.hasUpdate = gameHasUpdate(game, remoteById.value(game.id));
        updates += game.hasUpdate;
    }
    m_store->setGames(games);
    if (m_hooks.syncLibrary)
        m_hooks.syncLibrary();
    if (notify && m_hooks.notice)
        m_hooks.notice(updates ? QStringLiteral("%1 update(s) available").arg(updates)
                               : QStringLiteral("No updates"));
    return updates;
}

void GameUpdateService::checkUpdates(bool catalogAvailable)
{
    if (!catalogAvailable) {
        if (m_hooks.refreshCatalog)
            m_hooks.refreshCatalog();
        return;
    }
    recalculateLibraryUpdates(true);
}

void GameUpdateService::runAutoInstallUpdates()
{
    if (!m_settings->autoInstallUpdates() || !m_catalog)
        return;
    int started = 0;
    for (const LibraryGame& game : m_store->games()) {
        if (!game.hasUpdate || !game.autoUpdate
            || (m_hooks.entryPlayable && !m_hooks.entryPlayable(game.id))
            || (m_hooks.entryHasActiveJob && m_hooks.entryHasActiveJob(game.id)))
            continue;
        for (const CatalogEntry& entry : *m_catalog) {
            if (entry.id == game.id) {
                started += !m_jobs->startCatalogDownload(
                    entry, JobKind::Update,
                    game.libraryId.isEmpty() ? m_settings->defaultLibraryId() : game.libraryId).isEmpty();
                break;
            }
        }
    }
    if (started && m_hooks.notice)
        m_hooks.notice(QStringLiteral("Started %1 update(s)").arg(started));
}

} // namespace arachnel::core

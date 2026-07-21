#include "library_controller.h"

#include "catalog_model.h"
#include "file_utils.h"
#include "game_metadata_service.h"
#include "install_heuristics.h"
#include "job_store.h"
#include "launch_resolver.h"
#include "library_store.h"
#include "plugin_host.h"
#include "plugin_interface.h"
#include "settings_store.h"

#include <QDir>
#include <QFileInfo>

namespace arachnel::core {

LibraryController::LibraryController(LibraryModel* library, CatalogModel* catalog,
                                     LibraryStore* store, JobStore* jobs,
                                     SettingsStore* settings, PluginHost* plugins,
                                     GameMetadataService* metadata, Hooks hooks)
    : m_library(library), m_catalog(catalog), m_store(store), m_jobs(jobs), m_settings(settings),
      m_plugins(plugins), m_metadata(metadata), m_hooks(std::move(hooks))
{}

void LibraryController::sync() const
{
    if (m_hooks.syncLibrary)
        m_hooks.syncLibrary();
}

bool LibraryController::isEntryPlayable(const QString& entryId) const
{
    const LibraryGame* game = m_store->gameById(entryId);
    if (!game || game->installPath.isEmpty() || !QFileInfo::exists(game->installPath))
        return false;
    LaunchInfo info;
    if (m_plugins) {
        if (ISourcePlugin* plugin = m_plugins->plugin(game->sourceId))
            info = plugin->launchInfo(*game);
    }
    if (info.executable.isEmpty() && game->executableOverride.trimmed().isEmpty())
        info.executable = findGameExecutableInTree(game->installPath);
    const ResolvedLaunch resolved = resolveLaunch(info, *game, *m_settings);
    return !resolved.program.isEmpty() && QFileInfo::exists(resolved.program);
}

bool LibraryController::isEntryDownloadComplete(const QString& entryId) const
{
    for (const JobEntry& job : m_jobs->jobs()) {
        if (job.entryId == entryId && job.status == QStringLiteral("completed"))
            return true;
    }
    return false;
}

bool LibraryController::entryDownloadFilesExist(const QString& entryId) const
{
    for (const JobEntry& job : m_jobs->jobs()) {
        if (job.entryId == entryId && !job.savePath.isEmpty() && QDir(job.savePath).exists())
            return true;
    }
    const LibraryGame* game = m_store->gameById(entryId);
    return game && !game->downloadPath.isEmpty() && QDir(game->downloadPath).exists();
}

QVariantMap LibraryController::entryDetails(const QString& entryId) const
{
    QVariantMap info = m_library->gameInfo(entryId);
    const QVariantMap catalogInfo = m_catalog->entryInfo(entryId);
    if (info.isEmpty())
        info = catalogInfo;
    for (const QString& key : {QStringLiteral("description"), QStringLiteral("genres"),
                               QStringLiteral("sizeLabel"), QStringLiteral("coverUrl"),
                               QStringLiteral("version"), QStringLiteral("uploadDate"),
                               QStringLiteral("installKind"), QStringLiteral("installKindLabel"),
                               QStringLiteral("sourcePageUrl"), QStringLiteral("steamAppId"),
                               QStringLiteral("trailerUrl"), QStringLiteral("trailerThumbnailUrl")}) {
        if (info.value(key).toString().isEmpty() && catalogInfo.value(key).isValid())
            info.insert(key, catalogInfo.value(key));
    }
    const QString title = info.value(QStringLiteral("title")).toString();
    if (!title.isEmpty() && m_metadata) {
        const GameMetadata metadata = m_metadata->metadataForTitle(title);
        if (info.value(QStringLiteral("description")).toString().isEmpty()
            && !metadata.description.isEmpty())
            info.insert(QStringLiteral("description"), metadata.description);
    }
    const QString sourceId = info.value(QStringLiteral("sourceId")).toString();
    if (m_hooks.sourceWebsiteFor)
        info.insert(QStringLiteral("sourceWebsiteUrl"), m_hooks.sourceWebsiteFor(sourceId));
    const QString steamAppId = info.value(QStringLiteral("steamAppId")).toString();
    if (!steamAppId.isEmpty())
        info.insert(QStringLiteral("steamStoreUrl"),
                    QStringLiteral("https://store.steampowered.com/app/%1/").arg(steamAppId));
    if (const CatalogEntry* entry = m_hooks.findCatalogEntry ? m_hooks.findCatalogEntry(entryId) : nullptr) {
        info.insert(QStringLiteral("addonCount"), entry->addons.size());
        info.insert(QStringLiteral("hasAddons"), !entry->addons.isEmpty());
    }
    if (info.value(QStringLiteral("downloadPath")).toString().isEmpty() && m_hooks.findLatestJob) {
        if (const JobEntry* job = m_hooks.findLatestJob(entryId))
            info.insert(QStringLiteral("downloadPath"), job->savePath);
    }
    info.insert(QStringLiteral("installed"), isEntryPlayable(entryId));
    return info;
}

void LibraryController::setGameAutoUpdate(const QString& entryId, bool enabled)
{
    const LibraryGame* existing = m_store->gameById(entryId);
    if (!existing || existing->autoUpdate == enabled)
        return;
    LibraryGame game = *existing;
    game.autoUpdate = enabled;
    m_store->upsertGame(game);
    sync();
}

void LibraryController::setGameLaunchArgs(const QString& entryId, const QString& args)
{
    const LibraryGame* existing = m_store->gameById(entryId);
    if (!existing || existing->launchArgs == args)
        return;
    LibraryGame game = *existing;
    game.launchArgs = args;
    m_store->upsertGame(game);
    sync();
}

void LibraryController::setGameExecutableOverride(const QString& entryId, const QString& path)
{
    const LibraryGame* existing = m_store->gameById(entryId);
    if (!existing || existing->executableOverride == path)
        return;
    LibraryGame game = *existing;
    game.executableOverride = path;
    m_store->upsertGame(game);
    sync();
}

void LibraryController::setGameProtonId(const QString& entryId, const QString& protonId)
{
    const LibraryGame* existing = m_store->gameById(entryId);
    if (!existing || existing->protonId == protonId.trimmed())
        return;
    LibraryGame game = *existing;
    game.protonId = protonId.trimmed();
    m_store->upsertGame(game);
    sync();
}

void LibraryController::removeGame(const QString& gameId, bool deleteFiles)
{
    const LibraryGame* game = m_store->gameById(gameId);
    if (!game)
        return;
    const LibraryGame removed = *game;
    if (deleteFiles) {
        QString error;
        const QString libraryId = removed.libraryId.isEmpty() ? m_settings->defaultLibraryId()
                                                               : removed.libraryId;
        const QString gameDir = m_settings->gameDirFor(libraryId, gameId);
        if (!gameDir.isEmpty())
            removePathRecursive(gameDir, &error);
        if (!removed.installPath.isEmpty()
            && removed.installPath.compare(gameDir, Qt::CaseInsensitive) != 0)
            removePathRecursive(removed.installPath, &error);
        removePathRecursive(removed.downloadPath, &error);
    }
    m_store->removeGame(gameId);
    m_store->save();
    if (m_hooks.removeJobs)
        m_hooks.removeJobs(gameId);
    sync();
    if (m_hooks.notice)
        m_hooks.notice(QStringLiteral("Game removed: %1").arg(removed.title));
}

void LibraryController::removeEntry(const QString& entryId, bool deleteFiles)
{
    if (m_store->gameById(entryId)) {
        removeGame(entryId, deleteFiles);
        return;
    }
    if (m_hooks.removeJobs)
        m_hooks.removeJobs(entryId);
}

void LibraryController::moveGame(const QString& gameId, const QString& targetLibraryId)
{
    if (targetLibraryId.isEmpty()) {
        m_hooks.notice(QStringLiteral("No destination library selected"));
        return;
    }
    const LibraryGame* existing = m_store->gameById(gameId);
    if (!existing) {
        m_hooks.notice(QStringLiteral("Game not found"));
        return;
    }
    LibraryGame game = *existing;
    const QString sourceId = game.libraryId.isEmpty() ? m_settings->defaultLibraryId() : game.libraryId;
    if (sourceId == targetLibraryId) {
        m_hooks.notice(QStringLiteral("Game is already on this library"));
        return;
    }
    const QString sourceDir = m_settings->gameDirFor(sourceId, gameId);
    const QString targetDir = m_settings->gameDirFor(targetLibraryId, gameId);
    QString error;
    if (QDir(sourceDir).exists()) {
        if (!movePathRecursive(sourceDir, targetDir, &error)) {
            m_hooks.notice(QStringLiteral("Could not move: %1").arg(error));
            return;
        }
    } else if (!game.installPath.isEmpty() && QDir(game.installPath).exists()) {
        QDir().mkpath(QFileInfo(targetDir).absolutePath());
        if (!movePathRecursive(game.installPath, targetDir, &error)) {
            m_hooks.notice(QStringLiteral("Could not move: %1").arg(error));
            return;
        }
    }
    game.libraryId = targetLibraryId;
    game.installPath = relocatePathPrefix(game.installPath, sourceDir, targetDir);
    game.executableOverride = relocatePathPrefix(game.executableOverride, sourceDir, targetDir);
    game.downloadPath = relocatePathPrefix(game.downloadPath, m_settings->resolvedDownloadsRoot(sourceId),
                                           m_settings->resolvedDownloadsRoot(targetLibraryId));
    m_store->upsertGame(game);
    sync();
    m_hooks.notice(QStringLiteral("Game moved: %1").arg(game.title));
}

QVariantList LibraryController::gamesOnLibrary(const QString& libraryId) const
{
    const QString id = libraryId.isEmpty() ? m_settings->defaultLibraryId() : libraryId;
    QVariantList rows;
    for (const LibraryGame& game : m_store->games()) {
        if ((game.libraryId.isEmpty() ? m_settings->defaultLibraryId() : game.libraryId) != id)
            continue;
        QVariantMap row;
        row.insert(QStringLiteral("gameId"), game.id);
        row.insert(QStringLiteral("title"), game.title);
        row.insert(QStringLiteral("coverUrl"), game.coverUrl);
        row.insert(QStringLiteral("sizeLabel"), game.sizeLabel);
        row.insert(QStringLiteral("installPath"), game.installPath);
        row.insert(QStringLiteral("version"), game.version);
        rows.append(row);
    }
    return rows;
}

} // namespace arachnel::core

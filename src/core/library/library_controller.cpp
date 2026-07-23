#include "library_controller.h"

#include "catalog_model.h"
#include "file_utils.h"
#include "game_metadata_service.h"
#include "install_heuristics.h"
#include "install_kind.h"
#include "install_marker.h"
#include "job_status.h"
#include "job_store.h"
#include "launch_resolver.h"
#include "library_store.h"
#include "online_fix_overlay.h"
#include "plugin_host.h"
#include "plugin_interface.h"
#include "settings_store.h"
#include "source_plugin_model.h"
#include "storage_library.h"

#include <QCoreApplication>
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
    for (const JobEntry& job : m_jobs->jobs()) {
        if (job.entryId == entryId && !isJobTerminal(job.status))
            return false;
    }
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

    const QString installPath = info.value(QStringLiteral("installPath")).toString();
    const QVariantMap fixInfo = onlineFixOverlayInfo(installPath);
    for (auto it = fixInfo.constBegin(); it != fixInfo.constEnd(); ++it)
        info.insert(it.key(), it.value());
    const int installKind = info.value(QStringLiteral("installKind")).toInt();
    const bool catalogWantsFix = installKind == static_cast<int>(InstallKind::BundledFix)
        || installKind == static_cast<int>(InstallKind::FixDownload);
    info.insert(QStringLiteral("onlineFixRelevant"),
                fixInfo.value(QStringLiteral("onlineFixPresent")).toBool() || catalogWantsFix);
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

void LibraryController::setGameOnlineFixEnabled(const QString& entryId, bool enabled)
{
    const LibraryGame* existing = m_store->gameById(entryId);
    if (!existing || existing->installPath.isEmpty())
        return;
    QString error;
    if (!setOnlineFixOverlayEnabled(existing->installPath, enabled, &error)) {
        if (m_hooks.notice && !error.isEmpty())
            m_hooks.notice(error);
        return;
    }
    sync();
}

void LibraryController::removeGame(const QString& gameId, bool deleteFiles)
{
    const LibraryGame* game = m_store->gameById(gameId);
    if (!game)
        return;
    const LibraryGame removed = *game;

    QStringList pathsToDelete;
    if (deleteFiles) {
        const QString libraryId = removed.libraryId.isEmpty() ? m_settings->defaultLibraryId()
                                                               : removed.libraryId;
        const QString gameDir = m_settings->gameDirFor(libraryId, gameId);
        auto appendUnique = [&pathsToDelete](const QString& path) {
            if (path.isEmpty())
                return;
            for (const QString& existing : pathsToDelete) {
                if (existing.compare(path, Qt::CaseInsensitive) == 0)
                    return;
            }
            pathsToDelete.append(path);
        };
        appendUnique(gameDir);
        appendUnique(removed.installPath);
        appendUnique(removed.downloadPath);
    }

    m_store->removeGame(gameId);
    m_store->save();
    if (m_hooks.removeJobs)
        m_hooks.removeJobs(gameId);
    sync();

    if (pathsToDelete.isEmpty()) {
        if (m_hooks.notice)
            m_hooks.notice(QCoreApplication::translate("Core", "Game removed: %1").arg(removed.title));
        return;
    }

    // Drop from library immediately; wipe folders on a worker thread so the UI stays responsive.
    if (m_hooks.deleteGameFilesAsync) {
        if (m_hooks.notice) {
            m_hooks.notice(
                QCoreApplication::translate("Core", "Removing “%1”…").arg(removed.title));
        }
        m_hooks.deleteGameFilesAsync(pathsToDelete, removed.title);
        return;
    }

    QString error;
    for (const QString& path : pathsToDelete)
        removePathRecursive(path, &error);
    if (m_hooks.notice) {
        if (!error.isEmpty())
            m_hooks.notice(error);
        else
            m_hooks.notice(QCoreApplication::translate("Core", "Game removed: %1").arg(removed.title));
    }
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

bool LibraryController::removeStorageLibrary(const QString& libraryId, bool force)
{
    if (!m_settings || libraryId.isEmpty())
        return false;

    auto* storage = m_settings->storageLibraries();
    if (!storage || storage->count() <= 1)
        return false;

    int gameCount = 0;
    for (const LibraryGame& game : m_store->games()) {
        const QString gid = game.libraryId.isEmpty() ? m_settings->defaultLibraryId() : game.libraryId;
        if (gid == libraryId)
            ++gameCount;
    }

    if (gameCount > 0 && !force)
        return false;

    QString fallbackId;
    for (const StorageLibrary& library : storage->libraries()) {
        if (library.id != libraryId) {
            fallbackId = library.id;
            if (library.isDefault)
                break;
        }
    }
    if (fallbackId.isEmpty())
        return false;

    if (gameCount > 0) {
        for (LibraryGame game : m_store->games()) {
            const QString gid =
                game.libraryId.isEmpty() ? m_settings->defaultLibraryId() : game.libraryId;
            if (gid != libraryId)
                continue;
            // Keep install paths as-is; only detach from the removed drive entry.
            game.libraryId = fallbackId;
            m_store->upsertGame(game);
        }
        m_store->save();
        sync();
    }

    if (!storage->removeLibrary(libraryId))
        return false;

    if (m_hooks.notice) {
        if (gameCount > 0) {
            m_hooks.notice(
                QCoreApplication::translate("Core",
                                            "Drive removed. %1 game(s) kept on disk and listed "
                                            "under another drive.")
                    .arg(gameCount));
        } else {
            m_hooks.notice(QCoreApplication::translate("Core", "Drive removed"));
        }
    }
    return true;
}

namespace {

QString guessSourceIdFromFolder(const QString& folderName)
{
    const int dash = folderName.indexOf(QLatin1Char('-'));
    if (dash <= 0)
        return {};
    return folderName.left(dash);
}

QString titleFromFolderName(const QString& folderName, const QString& sourceId)
{
    QString rest = folderName;
    if (!sourceId.isEmpty() && rest.startsWith(sourceId + QLatin1Char('-'), Qt::CaseInsensitive))
        rest = rest.mid(sourceId.size() + 1);
    rest.replace(QLatin1Char('-'), QLatin1Char(' '));
    rest = rest.simplified();
    if (rest.isEmpty())
        return folderName;
    QStringList words = rest.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (QString& word : words) {
        if (word.size() == 1)
            word = word.toUpper();
        else
            word = word.left(1).toUpper() + word.mid(1);
    }
    return words.join(QLatin1Char(' '));
}

QString pluginDisplayName(PluginHost* plugins, const QString& sourceId)
{
    if (!plugins || sourceId.isEmpty())
        return sourceId;
    for (const SourcePluginInfo& info : plugins->pluginInfos()) {
        if (info.id == sourceId)
            return info.name.isEmpty() ? sourceId : info.name;
    }
    return sourceId;
}

bool installPathTaken(const LibraryStore* store, const QString& installPath)
{
    const QString clean = QDir::cleanPath(installPath);
    for (const LibraryGame& game : store->games()) {
        if (game.installPath.isEmpty())
            continue;
        if (QDir::cleanPath(game.installPath).compare(clean, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

} // namespace

int LibraryController::scanInstalledGames()
{
    if (!m_settings || !m_store)
        return 0;

    // Stamp known library installs so re-scan keeps only Arachnel-managed folders.
    for (const LibraryGame& game : m_store->games()) {
        if (game.installPath.isEmpty() || !QFileInfo::exists(game.installPath))
            continue;
        if (!hasInstallMarker(game.installPath))
            writeInstallMarker(game.installPath, game.id, game.sourceId);
    }

    int added = 0;
    for (const StorageLibrary& library : m_settings->storageLibraries()->libraries()) {
        const QString rootPath = normalizedStoragePath(library.path);
        if (rootPath.isEmpty() || !QFileInfo::exists(rootPath))
            continue;

        QDir root(rootPath);
        const QFileInfoList dirs =
            root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo& dirInfo : dirs) {
            const QString folderName = dirInfo.fileName();
            if (folderName.compare(QStringLiteral("downloads"), Qt::CaseInsensitive) == 0)
                continue;

            const QString installPath = QDir::cleanPath(dirInfo.absoluteFilePath());
            if (m_store->gameById(folderName) || installPathTaken(m_store, installPath))
                continue;

            // Only folders Arachnel itself installed (marker written on commit).
            if (!hasInstallMarker(installPath))
                continue;

            const QString executable = findGameExecutableInTree(installPath);
            if (executable.isEmpty())
                continue;

            const QString sourceId = guessSourceIdFromFolder(folderName);
            LibraryGame game;
            game.id = folderName;
            game.installPath = installPath;
            game.executableOverride = executable;
            game.libraryId = library.id;
            game.sourceId = sourceId;
            game.sourceName = pluginDisplayName(m_plugins, sourceId);
            game.installKind = InstallKind::PortableArchive;

            if (m_hooks.findCatalogEntry) {
                if (const CatalogEntry* entry = m_hooks.findCatalogEntry(folderName)) {
                    game.title = entry->title;
                    game.coverUrl = entry->coverUrl;
                    game.sourceId = entry->sourceId.isEmpty() ? sourceId : entry->sourceId;
                    game.sourceName = pluginDisplayName(m_plugins, game.sourceId);
                    game.version = entry->version;
                    game.description = entry->description;
                    game.genres = entry->genres;
                    game.sizeLabel = entry->sizeLabel;
                    game.uploadDate = entry->uploadDate;
                    game.magnetUri = entry->magnetUris.value(0);
                    game.steamAppId = entry->steamAppId;
                    game.installKind = entry->installKind;
                    QVector<InstalledComponent> components;
                    components.reserve(entry->addons.size());
                    for (const auto& addon : entry->addons)
                        components.append({addon.id, addon.title, addon.uploadDate, false});
                    game.components = components;
                }
            }

            if (game.title.isEmpty())
                game.title = titleFromFolderName(folderName, game.sourceId);

            if (m_hooks.detectInstallKind)
                game.installKind = m_hooks.detectInstallKind(game.sourceId, installPath);

            if (game.steamAppId.isEmpty() && m_metadata) {
                const GameMetadata meta = m_metadata->metadataForTitle(game.title);
                game.steamAppId = meta.steamAppId;
                if (game.coverUrl.isEmpty() && !meta.coverUrl.isEmpty())
                    game.coverUrl = meta.coverUrl;
            }

            m_store->upsertGame(game);
            ++added;
        }
    }

    if (added > 0) {
        m_store->save();
        sync();
    }
    return added;
}

} // namespace arachnel::core

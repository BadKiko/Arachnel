#include "library_controller.h"

#include "catalog_genre_normalize.h"
#include "catalog_model.h"
#include "catalog_types.h"
#include "file_utils.h"
#include "game_metadata_service.h"
#include "install_heuristics.h"
#include "install_kind.h"
#include "install_marker.h"
#include "job_kind.h"
#include "job_status.h"
#include "job_store.h"
#include "library_store.h"
#include "online_fix_overlay.h"
#include "plugin_host.h"
#include "plugin_interface.h"
#include "settings_store.h"
#include "source_plugin_model.h"
#include "steamless_service.h"
#include "storage_library.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

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
    for (const JobEntry& job : m_jobs->jobs()) {
        if (job.entryId != entryId || isJobTerminal(job.status))
            continue;
        // Parent job goes back to "installing" for optional addons after the game is committed.
        if (job.status == QStringLiteral("installing") && game && !game->installPath.isEmpty()
            && QFileInfo::exists(game->installPath))
            continue;
        return false;
    }
    if (!game || game->installPath.isEmpty() || !QFileInfo::exists(game->installPath))
        return false;

    // Never call plugin->launchInfo here. QML binds isEntryPlayable / entryDetails on every
    // frame; steamidra launchInfo walks the install tree and repairs Online Fix - UI hangs.
    QString overridePath = game->executableOverride.trimmed();
    if (!overridePath.isEmpty()
        && isExcludedGameExecutable(QFileInfo(overridePath).fileName())) {
        overridePath.clear();
    }
    if (!overridePath.isEmpty())
        return QFileInfo::exists(overridePath);

    // Steamidra depot installs must have a local Windows exe. Do not treat
    // steamAppId alone as playable - that used to enable Play and fall back to
    // steam://rungameid (Store license error) on empty/broken installs.
    if (game->sourceId == QStringLiteral("steamidra")) {
        const QString markerPath = game->installPath + QStringLiteral("/.arachnel-steamidra");
        QFile marker(markerPath);
        if (marker.open(QIODevice::ReadOnly)) {
            const QJsonObject launch =
                QJsonDocument::fromJson(marker.readAll()).object().value(QStringLiteral("launch")).toObject();
            const QString type = launch.value(QStringLiteral("type")).toString();
            if (type == QLatin1String("exe")) {
                const QString path = launch.value(QStringLiteral("path")).toString().trimmed();
                if (!path.isEmpty() && QFileInfo::exists(path))
                    return true;
            }
            if (type == QLatin1String("steam") || type == QLatin1String("missing"))
                return !findGameExecutableInTree(game->installPath).isEmpty();
        }
    }

    return !findGameExecutableInTree(game->installPath).isEmpty();
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

    auto asStringList = [](const QVariant& value) -> QStringList {
        if (value.metaType().id() == QMetaType::QStringList)
            return value.toStringList();
        if (value.canConvert<QStringList>())
            return value.toStringList();
        if (value.metaType().id() == QMetaType::QVariantList) {
            QStringList out;
            const QVariantList list = value.toList();
            out.reserve(list.size());
            for (const QVariant& item : list) {
                const QString s = item.toString();
                if (!s.isEmpty())
                    out.append(s);
            }
            return out;
        }
        return {};
    };

    for (const QString& key : {QStringLiteral("description"), QStringLiteral("genres"),
                               QStringLiteral("sizeLabel"), QStringLiteral("coverUrl"),
                               QStringLiteral("version"), QStringLiteral("uploadDate"),
                               QStringLiteral("installKind"), QStringLiteral("installKindLabel"),
                               QStringLiteral("sourcePageUrl"), QStringLiteral("steamAppId"),
                               QStringLiteral("trailerUrl"), QStringLiteral("trailerThumbnailUrl")}) {
        if (info.value(key).toString().isEmpty() && catalogInfo.value(key).isValid())
            info.insert(key, catalogInfo.value(key));
    }
    // Library rows don't store media URLs - keep catalog/Steam enrichments after install starts.
    if (asStringList(info.value(QStringLiteral("screenshotUrls"))).isEmpty()) {
        const QStringList shots = asStringList(catalogInfo.value(QStringLiteral("screenshotUrls")));
        if (!shots.isEmpty())
            info.insert(QStringLiteral("screenshotUrls"), QVariant::fromValue(shots));
    }
    const QString title = info.value(QStringLiteral("title")).toString();
    if (!title.isEmpty() && m_metadata) {
        const GameMetadata metadata = m_metadata->metadataForTitle(title);
        if (info.value(QStringLiteral("description")).toString().isEmpty()
            && !metadata.description.isEmpty())
            info.insert(QStringLiteral("description"), metadata.description);
        if (asStringList(info.value(QStringLiteral("screenshotUrls"))).isEmpty()
            && !metadata.screenshotUrls.isEmpty())
            info.insert(QStringLiteral("screenshotUrls"),
                        QVariant::fromValue(metadata.screenshotUrls));
        if (info.value(QStringLiteral("trailerUrl")).toString().isEmpty()
            && !metadata.trailerUrl.isEmpty())
            info.insert(QStringLiteral("trailerUrl"), metadata.trailerUrl);
        if (info.value(QStringLiteral("trailerThumbnailUrl")).toString().isEmpty()
            && !metadata.trailerThumbnailUrl.isEmpty())
            info.insert(QStringLiteral("trailerThumbnailUrl"), metadata.trailerThumbnailUrl);
    }
    const QString sourceId = info.value(QStringLiteral("sourceId")).toString();
    if (m_hooks.sourceWebsiteFor)
        info.insert(QStringLiteral("sourceWebsiteUrl"), m_hooks.sourceWebsiteFor(sourceId));
    const QString steamAppId = info.value(QStringLiteral("steamAppId")).toString();
    if (!steamAppId.isEmpty())
        info.insert(QStringLiteral("steamStoreUrl"),
                    QStringLiteral("https://store.steampowered.com/app/%1/").arg(steamAppId));
    if (const CatalogEntry* entry = m_hooks.findCatalogEntry ? m_hooks.findCatalogEntry(entryId) : nullptr) {
        int addonCount = entry->addons.size();
        if (entry->sourceId == QStringLiteral("steamidra")) {
            addonCount = 0;
            for (const CatalogComponent& c : entry->addons) {
                if (!isSteamStoreDlcId(c.id))
                    continue;
                if (c.kind != CatalogItemKind::Dlc && c.kind != CatalogItemKind::Addon)
                    continue;
                ++addonCount;
            }
        }
        // Catalog stays id-light until /dlcs fills addons - fall back to relay count.
        if (addonCount == 0 && entry->dlcCount > 0)
            addonCount = entry->dlcCount;
        info.insert(QStringLiteral("addonCount"), addonCount);
        info.insert(QStringLiteral("hasAddons"), addonCount > 0);
        info.insert(QStringLiteral("hasWorkshop"), entry->hasWorkshop);
        info.insert(QStringLiteral("hasDrm"), entry->hasDrm);
        if (info.value(QStringLiteral("genres")).toString().isEmpty())
            info.insert(QStringLiteral("genres"), genreLabelsFromBits(entry->genreBits));
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

    const QVariantMap steamlessInfo = SteamlessService::installInfo(installPath);
    for (auto it = steamlessInfo.constBegin(); it != steamlessInfo.constEnd(); ++it)
        info.insert(it.key(), it.value());

    if (const LibraryGame* game = m_store ? m_store->gameById(entryId) : nullptr) {
        if (!game->installPath.isEmpty() && QFileInfo::exists(game->installPath)) {
            QString defaultExe;
            if (m_plugins) {
                if (ISourcePlugin* plugin = m_plugins->plugin(game->sourceId)) {
                    if (!game->selectedLaunchOptionId.isEmpty()) {
                        for (const auto& opt : plugin->launchOptions(*game)) {
                            if (opt.id == game->selectedLaunchOptionId && !opt.executable.isEmpty()
                                && QFileInfo::exists(opt.executable)) {
                                defaultExe = opt.executable;
                                break;
                            }
                        }
                    }
                    if (defaultExe.isEmpty()) {
                        const auto opts = plugin->launchOptions(*game);
                        for (const auto& opt : opts) {
                            if (opt.isDefault && !opt.executable.isEmpty() && QFileInfo::exists(opt.executable)) {
                                defaultExe = opt.executable;
                                break;
                            }
                        }
                        if (defaultExe.isEmpty() && !opts.isEmpty() && !opts.first().executable.isEmpty()
                            && QFileInfo::exists(opts.first().executable)) {
                            defaultExe = opts.first().executable;
                        }
                    }
                    if (defaultExe.isEmpty()) {
                        const LaunchInfo li = plugin->launchInfo(*game);
                        if (!li.executable.isEmpty() && QFileInfo::exists(li.executable)
                            && !isExcludedGameExecutable(QFileInfo(li.executable).fileName())) {
                            defaultExe = li.executable;
                        }
                    }
                }
            }
            if (defaultExe.isEmpty()) {
                const QString markerPath = game->installPath + QStringLiteral("/.arachnel-steamidra");
                if (QFileInfo::exists(markerPath)) {
                    QFile f(markerPath);
                    if (f.open(QIODevice::ReadOnly)) {
                        const QJsonObject rootObj = QJsonDocument::fromJson(f.readAll()).object();
                        const QJsonArray arr = rootObj.value(QStringLiteral("launchOptions")).toArray();
                        for (const auto& v : arr) {
                            if (!v.isObject())
                                continue;
                            const QJsonObject o = v.toObject();
                            QString optExe = o.value(QStringLiteral("path")).toString();
                            if (optExe.isEmpty())
                                optExe = o.value(QStringLiteral("executable")).toString();
                            if (!optExe.isEmpty() && QFileInfo::exists(optExe)
                                && !isExcludedGameExecutable(QFileInfo(optExe).fileName())) {
                                const QString optId = o.value(QStringLiteral("id")).toString();
                                if (!game->selectedLaunchOptionId.isEmpty() && optId == game->selectedLaunchOptionId) {
                                    defaultExe = optExe;
                                    break;
                                }
                                if (defaultExe.isEmpty() || o.value(QStringLiteral("isDefault")).toBool(false)) {
                                    defaultExe = optExe;
                                }
                            }
                        }
                    }
                }
            }
            if (defaultExe.isEmpty()) {
                defaultExe = findGameExecutableInTree(game->installPath, game->title);
            }

            if (!defaultExe.isEmpty()) {
                info.insert(QStringLiteral("defaultExecutable"), defaultExe);
                info.insert(QStringLiteral("defaultExecutableName"), QFileInfo(defaultExe).fileName());
            }
        }
    }

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
    if (!existing)
        return;
    QString cleaned = path.trimmed();
    if (!cleaned.isEmpty()
        && isExcludedGameExecutable(QFileInfo(cleaned).fileName())) {
        cleaned.clear();
    }
    if (existing->executableOverride == cleaned)
        return;
    LibraryGame game = *existing;
    game.executableOverride = cleaned;
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
    // Refresh DLC unlocks so SmokeAPI stays out from under Online Fix.
    if (m_plugins) {
        if (ISourcePlugin* plugin = m_plugins->plugin(existing->sourceId)) {
            QStringList enabledIds;
            enabledIds.reserve(existing->components.size());
            for (const InstalledComponent& component : existing->components) {
                if (component.installed && component.enabled)
                    enabledIds.append(component.id);
            }
            if (!plugin->applySelectedDlc(*existing, enabledIds) && m_hooks.notice) {
                m_hooks.notice(
                    QCoreApplication::translate("Core", "Couldn't update DLC unlocks."));
            }
        }
    }
    sync();
}

void LibraryController::setGameAddonEnabled(const QString& entryId, const QString& addonId,
                                            bool enabled)
{
    const LibraryGame* existing = m_store->gameById(entryId);
    if (!existing || addonId.isEmpty())
        return;

    LibraryGame game = *existing;
    bool found = false;
    for (InstalledComponent& component : game.components) {
        if (component.id != addonId)
            continue;
        if (!component.installed)
            return;
        if (component.enabled == enabled)
            return;
        component.enabled = enabled;
        found = true;
        break;
    }
    if (!found)
        return;

    QStringList enabledIds;
    enabledIds.reserve(game.components.size());
    for (const InstalledComponent& component : game.components) {
        if (component.installed && component.enabled)
            enabledIds.append(component.id);
    }

    if (m_plugins) {
        if (ISourcePlugin* plugin = m_plugins->plugin(game.sourceId)) {
            if (!plugin->applySelectedDlc(game, enabledIds)) {
                if (m_hooks.notice) {
                    m_hooks.notice(
                        QCoreApplication::translate("Core", "Couldn't update DLC unlocks."));
                }
                return;
            }
        }
    }

    m_store->upsertGame(game);
    sync();
}

void LibraryController::healInstalledAddons(const QString& entryId)
{
    const LibraryGame* existing = m_store->gameById(entryId);
    if (!existing || existing->installPath.isEmpty() || existing->components.isEmpty())
        return;
    if (!QFileInfo::exists(existing->installPath))
        return;

    QSet<QString> fromMarker;
    const QString markerPath = existing->installPath + QStringLiteral("/.arachnel-steamidra");
    QFile marker(markerPath);
    if (marker.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(marker.readAll()).object();
        for (const QJsonValue& v : root.value(QStringLiteral("selectedDlc")).toArray()) {
            const QString id = v.toString();
            if (!id.isEmpty())
                fromMarker.insert(id);
        }
    }

    LibraryGame game = *existing;
    bool changed = false;
    for (InstalledComponent& component : game.components) {
        if (component.installed)
            continue;
        // Only ids listed in the install marker were actually selected.
        // Empty selectedDlc means "no DLC" - never mark everything installed.
        if (!fromMarker.isEmpty() && fromMarker.contains(component.id)) {
            component.installed = true;
            changed = true;
        }
    }
    if (changed) {
        m_store->upsertGame(game);
        sync();
    }

    // Always push current enabled set into Steam lua (fixes toggles that only flipped UI).
    QStringList enabledIds;
    for (const InstalledComponent& component : game.components) {
        if (component.installed && component.enabled)
            enabledIds.append(component.id);
    }
    if (m_plugins) {
        if (ISourcePlugin* plugin = m_plugins->plugin(game.sourceId)) {
            if (!plugin->applySelectedDlc(game, enabledIds) && m_hooks.notice) {
                m_hooks.notice(
                    QCoreApplication::translate("Core", "Couldn't update DLC unlocks."));
            }
        }
    }
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
                QCoreApplication::translate("Core", "Removing %1...").arg(removed.title));
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
        m_hooks.notice(QCoreApplication::translate("Core", "No destination library selected"));
        return;
    }
    const LibraryGame* existing = m_store->gameById(gameId);
    if (!existing) {
        m_hooks.notice(QCoreApplication::translate("Core", "Game not found"));
        return;
    }
    LibraryGame game = *existing;
    const QString sourceId = game.libraryId.isEmpty() ? m_settings->defaultLibraryId() : game.libraryId;
    if (sourceId == targetLibraryId) {
        m_hooks.notice(QCoreApplication::translate("Core", "Game is already on this library"));
        return;
    }

    for (const JobEntry& job : m_jobs->jobs()) {
        if (job.entryId != gameId || job.kind != JobKind::Move || isJobTerminal(job.status))
            continue;
        m_hooks.notice(QCoreApplication::translate("Core", "Already moving: %1").arg(game.title));
        return;
    }

    const QString sourceDir = m_settings->gameDirFor(sourceId, gameId);
    const QString targetDir = m_settings->gameDirFor(targetLibraryId, gameId);

    QString fromPath;
    if (QDir(sourceDir).exists())
        fromPath = sourceDir;
    else if (!game.installPath.isEmpty() && QDir(game.installPath).exists())
        fromPath = game.installPath;

    if (fromPath.isEmpty()) {
        // Metadata-only reassign (files already gone / never on disk).
        finalizeMovedGame(gameId, targetLibraryId, sourceDir, targetDir);
        m_hooks.notice(QCoreApplication::translate("Core", "Game moved: %1").arg(game.title));
        return;
    }

    if (!m_hooks.moveGameAsync) {
        QString error;
        QDir().mkpath(QFileInfo(targetDir).absolutePath());
        if (!movePathRecursive(fromPath, targetDir, &error)) {
            m_hooks.notice(QCoreApplication::translate("Core", "Could not move: %1").arg(error));
            return;
        }
        finalizeMovedGame(gameId, targetLibraryId, fromPath, targetDir);
        m_hooks.notice(QCoreApplication::translate("Core", "Game moved: %1").arg(game.title));
        return;
    }

    MoveGameWork work;
    work.gameId = gameId;
    work.title = game.title;
    work.coverUrl = game.coverUrl;
    work.sourceId = game.sourceId;
    work.fromPath = fromPath;
    work.toPath = targetDir;
    work.sourceLibraryId = sourceId;
    work.targetLibraryId = targetLibraryId;
    m_hooks.notice(QCoreApplication::translate("Core", "Moving %1…").arg(game.title));
    m_hooks.moveGameAsync(work);
}

void LibraryController::finalizeMovedGame(const QString& gameId, const QString& targetLibraryId,
                                          const QString& fromPath, const QString& toPath)
{
    const LibraryGame* existing = m_store->gameById(gameId);
    if (!existing)
        return;

    LibraryGame game = *existing;
    const QString sourceId = game.libraryId.isEmpty() ? m_settings->defaultLibraryId() : game.libraryId;
    game.libraryId = targetLibraryId;
    game.installPath = relocatePathPrefix(game.installPath, fromPath, toPath);
    if (game.installPath.isEmpty() || !QDir(game.installPath).exists())
        game.installPath = toPath;
    game.executableOverride = relocatePathPrefix(game.executableOverride, fromPath, toPath);
    game.downloadPath =
        relocatePathPrefix(game.downloadPath, m_settings->resolvedDownloadsRoot(sourceId),
                           m_settings->resolvedDownloadsRoot(targetLibraryId));

    // Absolute launch paths inside steamidra marker must follow the folder.
    rewritePathPrefixInFile(toPath + QStringLiteral("/.arachnel-steamidra"), fromPath, toPath);

    m_store->upsertGame(game);
    sync();
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
    return commitScanCandidates(discoverInstallCandidates());
}

QVector<LibraryController::ScanCandidate> LibraryController::discoverInstallCandidates() const
{
    QVector<ScanCandidate> out;
    if (!m_settings || !m_store)
        return out;

    QSet<QString> knownIds;
    QSet<QString> knownPaths;
    for (const LibraryGame& game : m_store->games()) {
        knownIds.insert(game.id);
        if (!game.installPath.isEmpty())
            knownPaths.insert(QDir::cleanPath(game.installPath).toLower());
    }

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
            if (knownIds.contains(folderName)
                || knownPaths.contains(installPath.toLower()))
                continue;

            // Only folders Arachnel itself installed (marker written on commit).
            if (!hasInstallMarker(installPath))
                continue;

            const QString executable = findGameExecutableInTree(installPath);
            if (executable.isEmpty())
                continue;

            ScanCandidate c;
            c.folderName = folderName;
            c.installPath = installPath;
            c.executable = executable;
            c.libraryId = library.id;
            c.sourceId = guessSourceIdFromFolder(folderName);
            out.append(std::move(c));
        }
    }
    return out;
}

int LibraryController::commitScanCandidates(const QVector<ScanCandidate>& candidates)
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
    for (const ScanCandidate& c : candidates) {
        if (m_store->gameById(c.folderName) || installPathTaken(m_store, c.installPath))
            continue;

        LibraryGame game;
        game.id = c.folderName;
        game.installPath = c.installPath;
        game.executableOverride = c.executable;
        game.libraryId = c.libraryId;
        game.sourceId = c.sourceId;
        game.sourceName = pluginDisplayName(m_plugins, c.sourceId);
        game.installKind = InstallKind::PortableArchive;

        if (m_hooks.findCatalogEntry) {
            if (const CatalogEntry* entry = m_hooks.findCatalogEntry(c.folderName)) {
                game.title = entry->title;
                if (entry->coverUrl.startsWith(QStringLiteral("file:")))
                    game.coverUrl = entry->coverUrl;
                game.sourceId = entry->sourceId.isEmpty() ? c.sourceId : entry->sourceId;
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
            game.title = titleFromFolderName(c.folderName, game.sourceId);

        if (m_hooks.detectInstallKind)
            game.installKind = m_hooks.detectInstallKind(game.sourceId, c.installPath);

        if (game.steamAppId.isEmpty() && m_metadata) {
            const GameMetadata meta = m_metadata->metadataForTitle(game.title);
            game.steamAppId = meta.steamAppId;
        }

        m_store->upsertGame(game);
        ++added;
    }

    if (added > 0) {
        m_store->save();
        sync();
    }
    return added;
}

} // namespace arachnel::core

#include "core_controller_impl.h"

#include "file_utils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace arachnel::core {

namespace {

QString workshopComponentId(const QString& publishedFileId)
{
    return QStringLiteral("workshop:%1").arg(publishedFileId);
}

QString workshopStagingRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/workshop-content");
}

QString workshopStagingDir(const QString& steamAppId, const QString& publishedFileId)
{
    return workshopStagingRoot() + QLatin1Char('/') + steamAppId + QLatin1Char('/')
           + publishedFileId;
}

QString workshopInstallDir(const QString& gameInstallPath, const QString& steamAppId,
                           const QString& publishedFileId)
{
    return gameInstallPath + QStringLiteral("/steamapps/workshop/content/") + steamAppId
           + QLatin1Char('/') + publishedFileId;
}

bool dirHasContent(const QString& path)
{
    if (path.isEmpty() || !QDir(path).exists())
        return false;
    const QStringList entries =
        QDir(path).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    return !entries.isEmpty();
}

void ensureSteamFixWorkshopEnabled(const QString& gameInstallPath, const QString& appid)
{
    if (gameInstallPath.isEmpty() || !QDir(gameInstallPath).exists())
        return;

    const QString iniPath = gameInstallPath + QStringLiteral("/SteamFix.ini");
    if (QFileInfo::exists(iniPath)) {
        QFile ini(iniPath);
        if (!ini.open(QIODevice::ReadOnly))
            return;
        QByteArray text = ini.readAll();
        ini.close();
        if (text.contains("Workshop=true"))
            return;
        text.replace("Workshop=false", "Workshop=true");
        if (!text.contains("Workshop=true")) {
            const int idx = text.indexOf("[Interfaces]");
            if (idx >= 0) {
                const int lineEnd = text.indexOf('\n', idx);
                if (lineEnd > 0)
                    text.insert(lineEnd + 1, "Workshop=true\r\n");
            }
        }
        QSaveFile out(iniPath);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(text);
            out.commit();
        }
        return;
    }

    QSaveFile f(iniPath);
    if (!f.open(QIODevice::WriteOnly))
        return;
    QByteArray t;
    t += "[Main]\r\nRealAppId=" + appid.toUtf8() + "\r\nFakeAppId=480\r\n\r\n";
    t += "[Misc]\r\nOverlay=false\r\n\r\n";
    t += "[Interfaces]\r\nApps=true\r\nWorkshop=true\r\n\r\n[DLC]\r\n0=dlc\r\n";
    f.write(t);
    f.commit();
}

void upsertWorkshopComponent(LibraryGame* game, const QString& publishedFileId,
                             const QString& title, bool installed, const QString& path)
{
    if (!game)
        return;
    const QString componentId = workshopComponentId(publishedFileId);
    for (InstalledComponent& c : game->components) {
        if (c.id == componentId) {
            c.title = title;
            c.uploadDate = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            c.installed = installed;
            c.path = path;
            return;
        }
    }
    InstalledComponent c;
    c.id = componentId;
    c.title = title;
    c.uploadDate = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    c.installed = installed;
    c.path = path;
    game->components.append(c);
}

QString resolveOwnsDownloadPlugin(PluginHost* host, const QString& preferredSourceId)
{
    if (!host)
        return {};
    if (host->pluginOwnsDownload(preferredSourceId))
        return preferredSourceId;
    if (host->pluginOwnsDownload(QStringLiteral("steamidra")))
        return QStringLiteral("steamidra");
    for (const SourcePluginInfo& info : host->pluginInfos()) {
        if (host->pluginOwnsDownload(info.id))
            return info.id;
    }
    return {};
}

} // namespace

void CoreController::requestWorkshopPage(const QString& steamAppId, int page)
{
    if (!m_workshopService)
        return;
    m_workshopService->requestPage(steamAppId, page);
}

QVariantList CoreController::workshopItems(const QString& steamAppId) const
{
    if (!m_workshopService)
        return {};
    return m_workshopService->itemsForApp(steamAppId);
}

void CoreController::requestWorkshopPreview(const QString& previewUrl)
{
    if (m_workshopService)
        m_workshopService->requestPreview(previewUrl);
}

void CoreController::releaseWorkshopPreview(const QString& previewUrl)
{
    if (m_workshopService)
        m_workshopService->releasePreview(previewUrl);
}

QString CoreController::workshopPreviewUrl(const QString& previewUrl) const
{
    if (!m_workshopService)
        return {};
    return m_workshopService->localPreviewUrl(previewUrl);
}

QString CoreController::workshopItemStatus(const QString& gameId,
                                           const QString& publishedFileId) const
{
    if (gameId.isEmpty() || publishedFileId.isEmpty())
        return {};

    const LibraryGame* game = m_libraryStore.gameById(gameId);
    if (!game)
        return {};

    const QString componentId = workshopComponentId(publishedFileId);
    for (const InstalledComponent& c : game->components) {
        if (c.id != componentId)
            continue;
        if (c.installed)
            return QStringLiteral("installed");
        if (!c.path.isEmpty() && dirHasContent(c.path))
            return QStringLiteral("cached");
    }

    if (!game->steamAppId.isEmpty() && !game->installPath.isEmpty()
        && dirHasContent(workshopInstallDir(game->installPath, game->steamAppId, publishedFileId)))
        return QStringLiteral("installed");

    if (!game->steamAppId.isEmpty()
        && dirHasContent(workshopStagingDir(game->steamAppId, publishedFileId)))
        return QStringLiteral("cached");

    return {};
}

void CoreController::downloadWorkshopItem(const QString& gameId, const QString& publishedFileId)
{
    if (gameId.isEmpty() || publishedFileId.isEmpty())
        return;

    const LibraryGame* game = m_libraryStore.gameById(gameId);
    if (!game) {
        showNotice(QCoreApplication::translate("Core", "Game not found in library"));
        return;
    }
    if (game->steamAppId.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "No Steam App ID for %1").arg(game->title));
        return;
    }
    if (!m_pluginHost || !m_jobOrchestrator) {
        showNotice(QCoreApplication::translate("Core", "Plugins are not ready"));
        return;
    }

    const QString sourceId = resolveOwnsDownloadPlugin(m_pluginHost, game->sourceId);
    if (sourceId.isEmpty()) {
        showNotice(QCoreApplication::translate("Core",
                                               "Steam plugin is required for Workshop downloads."));
        return;
    }

    ISourcePlugin* plugin = m_pluginHost->plugin(sourceId);
    if (!plugin) {
        showNotice(QCoreApplication::translate("Core", "Plugin not loaded: %1").arg(sourceId));
        return;
    }

    QString itemTitle = publishedFileId;
    qint64 estimatedBytes = 0;
    if (m_workshopService) {
        if (const auto item = m_workshopService->itemById(game->steamAppId, publishedFileId)) {
            itemTitle = item->title;
            estimatedBytes = item->fileSize;
        }
    }

    const bool hasInstall =
        !game->installPath.isEmpty() && QDir(game->installPath).exists();
    const QString installMode =
        hasInstall ? QStringLiteral("workshop") : QStringLiteral("workshop_stage");
    const QString destDir =
        hasInstall ? workshopInstallDir(game->installPath, game->steamAppId, publishedFileId)
                   : workshopStagingDir(game->steamAppId, publishedFileId);

    const QString jobId = m_jobOrchestrator->startWorkshopOwnedDownload(
        game->id, game->title, game->coverUrl, sourceId, game->steamAppId, publishedFileId,
        itemTitle, game->libraryId, estimatedBytes);
    if (jobId.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "Could not start download for %1")
                       .arg(itemTitle));
        return;
    }

    InstallContext ctx;
    ctx.jobId = jobId;
    ctx.entryId = publishedFileId;
    ctx.sourceId = sourceId;
    ctx.title = itemTitle;
    ctx.targetPath = hasInstall ? game->installPath : destDir;
    ctx.downloadsPath = m_settings.resolvedDownloadsRoot(game->libraryId);
    ctx.downloadPath = destDir;
    ctx.magnetUri = game->steamAppId;
    ctx.steamAppId = game->steamAppId;
    ctx.installMode = installMode;
    ctx.version = publishedFileId;

    m_pluginHost->runOwnedDownloadAsync(
        plugin, ctx,
        [this, jobId](const OwnedDownloadProgress& progress) {
            m_jobOrchestrator->reportPluginProgress(jobId, progress);
        },
        [this, jobId, gameId = game->id, publishedFileId, itemTitle, destDir,
         staged = !hasInstall](const InstallResult& result) {
            if (!result.success) {
                m_jobOrchestrator->failPluginDownload(
                    jobId, result.error.isEmpty()
                               ? QCoreApplication::translate("Core", "Install failed")
                               : result.error);
                return;
            }
            const QString resultPath =
                result.installPath.isEmpty() ? destDir : result.installPath;
            m_jobOrchestrator->completePluginDownload(jobId, resultPath);

            if (const LibraryGame* existing = m_libraryStore.gameById(gameId)) {
                LibraryGame updated = *existing;
                upsertWorkshopComponent(&updated, publishedFileId, itemTitle, !staged, resultPath);
                m_libraryStore.upsertGame(updated);
                if (!m_library.replaceGame(updated))
                    syncLibraryFromStore();
            }

            if (staged) {
                showNotice(QCoreApplication::translate(
                    "Core",
                    "Saved to Workshop cache - will attach when the game is installed."));
            }
        });
}

void CoreController::attachWorkshopItem(const QString& gameId, const QString& publishedFileId)
{
    attachWorkshopItemImpl(gameId, publishedFileId, false);
}

bool CoreController::attachWorkshopItemImpl(const QString& gameId, const QString& publishedFileId,
                                            bool quiet)
{
    if (gameId.isEmpty() || publishedFileId.isEmpty())
        return false;

    const LibraryGame* game = m_libraryStore.gameById(gameId);
    if (!game) {
        if (!quiet)
            showNotice(QCoreApplication::translate("Core", "Game not found in library"));
        return false;
    }
    if (game->steamAppId.isEmpty()) {
        if (!quiet)
            showNotice(QCoreApplication::translate("Core", "No Steam App ID for %1").arg(game->title));
        return false;
    }
    if (game->installPath.isEmpty() || !QDir(game->installPath).exists()) {
        if (!quiet)
            showNotice(QCoreApplication::translate(
                "Core", "Install the game before attaching Workshop files."));
        return false;
    }

    const QString componentId = workshopComponentId(publishedFileId);
    QString stagingPath = workshopStagingDir(game->steamAppId, publishedFileId);
    QString title = publishedFileId;
    for (const InstalledComponent& c : game->components) {
        if (c.id != componentId)
            continue;
        if (c.installed) {
            if (!quiet)
                showNotice(QCoreApplication::translate("Core", "Already attached: %1").arg(c.title));
            return false;
        }
        if (!c.path.isEmpty())
            stagingPath = c.path;
        if (!c.title.isEmpty())
            title = c.title;
        break;
    }

    if (!dirHasContent(stagingPath)) {
        if (!quiet)
            showNotice(QCoreApplication::translate("Core", "No cached Workshop files for %1")
                           .arg(title));
        return false;
    }

    const QString dest =
        workshopInstallDir(game->installPath, game->steamAppId, publishedFileId);
    QString copyError;
    if (!copyPathRecursive(stagingPath, dest, &copyError)) {
        if (!quiet) {
            showNotice(copyError.isEmpty()
                           ? QCoreApplication::translate("Core", "Could not attach Workshop files")
                           : copyError);
        }
        return false;
    }

    ensureSteamFixWorkshopEnabled(game->installPath, game->steamAppId);

    LibraryGame updated = *game;
    upsertWorkshopComponent(&updated, publishedFileId, title, true, dest);
    m_libraryStore.upsertGame(updated);
    if (!m_library.replaceGame(updated))
        syncLibraryFromStore();

    if (!quiet)
        showNotice(QCoreApplication::translate("Core", "Attached: %1").arg(title));
    return true;
}

void CoreController::attachPendingWorkshopItems(const QString& gameId)
{
    const LibraryGame* game = m_libraryStore.gameById(gameId);
    if (!game || game->steamAppId.isEmpty() || game->installPath.isEmpty()
        || !QDir(game->installPath).exists())
        return;

    QStringList pending;
    for (const InstalledComponent& c : game->components) {
        if (!c.id.startsWith(QStringLiteral("workshop:")))
            continue;
        if (c.installed)
            continue;
        pending.append(c.id.mid(QStringLiteral("workshop:").size()));
    }

    const QDir appStaging(workshopStagingRoot() + QLatin1Char('/') + game->steamAppId);
    if (appStaging.exists()) {
        for (const QString& id : appStaging.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (!pending.contains(id) && dirHasContent(appStaging.absoluteFilePath(id)))
                pending.append(id);
        }
    }

    int attached = 0;
    for (const QString& publishedFileId : pending) {
        if (attachWorkshopItemImpl(gameId, publishedFileId, true))
            ++attached;
    }
    if (attached > 0) {
        showNotice(QCoreApplication::translate("Core", "Attached %1 Workshop file(s)")
                       .arg(attached));
    }
}

} // namespace arachnel::core

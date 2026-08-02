#include "core_controller_impl.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace arachnel::core {

namespace {

QStringList variantListToStringList(const QVariantList& values)
{
    QStringList result;
    result.reserve(values.size());
    for (const QVariant& value : values) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty())
            result.append(text);
    }
    return result;
}

} // namespace

void CoreController::openExternalUrl(const QString& url)
{
    const QUrl parsed(url.trimmed());
    if (parsed.isValid())
        QDesktopServices::openUrl(parsed);
}

QString CoreController::applicationDataPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

bool CoreController::clearApplicationData()
{
    if (m_applicationDataCleared)
        return true;

    const QString dataDir = applicationDataPath();
    if (dataDir.isEmpty()
        || !dataDir.contains(QStringLiteral("Arachnel"), Qt::CaseInsensitive)) {
        showNotice(QCoreApplication::translate("Core", "Could not resolve application data folder"));
        return false;
    }

    // Stop I/O without rewriting jobs/settings into AppData.
    if (m_launchController)
        m_launchController->stopRunningGame();

    if (m_catalogValidateLoader)
        m_catalogValidateLoader->cancelActive();
    if (m_httpSession)
        m_httpSession->shutdown();
    if (m_torrentSession)
        m_torrentSession->shutdown();
    if (m_pluginHost)
        m_pluginHost->shutdownPlugins();

    QSettings appearanceSettings;
    appearanceSettings.clear();
    appearanceSettings.sync();

    if (QDir(dataDir).exists() && !QDir(dataDir).removeRecursively()) {
        showNotice(QCoreApplication::translate("Core", "Failed to delete application data"));
        return false;
    }

    // Seed a minimal settings file so the next launch shows first-run onboarding.
    if (!QDir().mkpath(dataDir)) {
        showNotice(QCoreApplication::translate("Core", "Failed to reset application data"));
        return false;
    }
    QFile settingsFile(dataDir + QStringLiteral("/settings.json"));
    if (settingsFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonObject obj;
        obj.insert(QStringLiteral("onboardingCompleted"), false);
        settingsFile.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        settingsFile.close();
    }

    m_applicationDataCleared = true;
    showNotice(QCoreApplication::translate(
        "Core", "Application data deleted. Arachnel will quit now."));
    QTimer::singleShot(400, qApp, []() { QCoreApplication::quit(); });
    return true;
}

QVariantList CoreController::installOffersForEntry(const QString& entryId) const
{
    if (!m_catalogController)
        return {};
    return m_catalogController->installOffersForEntry(entryId);
}

void CoreController::installCatalogEntryFromSource(const QString& entryId, const QString& sourceId,
                                                   const QString& libraryId,
                                                   const QVariantList& addonIdsVariant,
                                                   const QString& installMode)
{
    if (!m_catalogController) {
        installCatalogEntry(entryId, libraryId, addonIdsVariant, installMode);
        return;
    }
    const auto offer = m_catalogController->resolveInstallOffer(entryId, sourceId);
    if (!offer) {
        showNotice(QCoreApplication::translate("Core", "Catalog entry not found: %1").arg(entryId));
        return;
    }
    installCatalogEntry(offer->id, libraryId, addonIdsVariant, installMode);
}

void CoreController::installCatalogEntry(const QString& entryId, const QString& libraryId,
                                         const QVariantList& addonIdsVariant,
                                         const QString& installMode)
{
    if (!ensureProtonReady())
        return;

    const std::optional<CatalogEntry> entryOpt = resolveCatalogEntry(entryId);
    if (!entryOpt) {
        if (const LibraryGame* game = m_libraryStore.gameById(entryId)) {
            if (!game->sourceId.isEmpty())
                if (m_catalogController)
                    m_catalogController->requestCatalogLoad(game->sourceId);
        }
        showNotice(QCoreApplication::translate("Core", "Catalog entry not found: %1").arg(entryId));
        return;
    }

    const CatalogEntry& entry = *entryOpt;

    const bool ownsDownload =
        m_pluginHost && m_pluginHost->pluginOwnsDownload(entry.sourceId);

    if (!ownsDownload && entry.magnetUris.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "No download link for %1").arg(entry.title));
        return;
    }

    if (ownsDownload && entry.steamAppId.isEmpty() && entry.magnetUris.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "No Steam App ID for %1").arg(entry.title));
        return;
    }

    const QStringList addonIds = variantListToStringList(addonIdsVariant);
    const QString libId = libraryId.isEmpty() ? m_settings.defaultLibraryId() : libraryId;

    if (ownsDownload) {
        ISourcePlugin* plugin = m_pluginHost->plugin(entry.sourceId);
        if (!plugin) {
            showNotice(QCoreApplication::translate("Core", "Plugin not loaded: %1").arg(entry.sourceId));
            return;
        }

        const bool isUpdate =
            installMode.compare(QStringLiteral("update"), Qt::CaseInsensitive) == 0;
        const JobKind kind = isUpdate ? JobKind::Update : JobKind::Download;
        const QString jobId = m_jobOrchestrator->startPluginOwnedDownload(entry, kind, libId);
        if (jobId.isEmpty()) {
            showNotice(isUpdate
                           ? QCoreApplication::translate("Core", "Could not start update for %1")
                                 .arg(entry.title)
                           : QCoreApplication::translate("Core", "Could not start download for %1")
                                 .arg(entry.title));
            return;
        }

        ensureLibraryPlaceholder(entry, libId, addonIds);

        pruneUnselectedAddonJobs(entryId, addonIds);
        if (!addonIds.isEmpty())
            beginInstallSession(entryId, jobId, entry.sourceId, addonIds);
        for (const QString& addonId : addonIds) {
            const CatalogComponent* addon = findCatalogAddon(entry, addonId);
            if (!addon)
                continue;
            m_jobOrchestrator->startAddonDownload(entry, *addon);
        }

        const LibraryGame* existing = m_libraryStore.gameById(entry.id);
        InstallContext ctx;
        ctx.jobId = jobId;
        ctx.entryId = entry.id;
        ctx.sourceId = entry.sourceId;
        ctx.title = entry.title;
        ctx.targetPath =
            existing && !existing->installPath.isEmpty() && QDir(existing->installPath).exists()
                ? existing->installPath
                : m_settings.gameDirFor(libId, entry.id);
        ctx.downloadsPath = m_settings.resolvedDownloadsRoot(libId);
        ctx.downloadPath =
            ctx.downloadsPath + QLatin1Char('/')
            + (isUpdate ? QStringLiteral("update/") : QStringLiteral("install/")) + entry.id;
        ctx.magnetUri = entry.steamAppId;
        ctx.uploadDate = entry.uploadDate;
        ctx.version = entry.version;
        ctx.steamAppId = entry.steamAppId;
        ctx.installKind = entry.installKind;
        ctx.installMode = isUpdate ? QStringLiteral("update") : installMode;
        qInfo().noquote() << "[owns-download]" << entry.id
                          << "mode" << ctx.installMode
                          << "forceUpdate" << isUpdate
                          << "target" << ctx.targetPath;

        m_pluginHost->runOwnedDownloadAsync(
            plugin, ctx,
            [this, jobId](const OwnedDownloadProgress& progress) {
                m_jobOrchestrator->reportPluginProgress(jobId, progress);
            },
            [this, jobId](const InstallResult& result) {
                if (result.success)
                    m_jobOrchestrator->completePluginDownload(jobId, result.installPath);
                else
                    m_jobOrchestrator->failPluginDownload(
                        jobId, result.error.isEmpty()
                                   ? QCoreApplication::translate("Core", "Install failed")
                                   : result.error);
            });
        return;
    }

    const QString jobId = m_jobOrchestrator->startCatalogDownload(entry, JobKind::Download, libId);
    if (jobId.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "Could not start download for %1").arg(entry.title));
        return;
    }

    ensureLibraryPlaceholder(entry, libId, addonIds);

    pruneUnselectedAddonJobs(entryId, addonIds);

    if (!addonIds.isEmpty())
        beginInstallSession(entryId, jobId, entry.sourceId, addonIds);

    for (const QString& addonId : addonIds) {
        const CatalogComponent* addon = findCatalogAddon(entry, addonId);
        if (!addon)
            continue;
        m_jobOrchestrator->startAddonDownload(entry, *addon);
    }
}

bool CoreController::needsInstallLocationChoice() const
{
    return m_settings.storageLibraries()->count() > 1;
}

void CoreController::installCatalogAddon(const QString& entryId, const QString& addonId)
{
    const CatalogEntry* entry = findCatalogEntry(entryId);
    if (!entry) {
        showNotice(QCoreApplication::translate("Core", "Game not found: %1").arg(entryId));
        return;
    }

    const CatalogComponent* addon = findCatalogAddon(*entry, addonId);
    if (!addon) {
        showNotice(QCoreApplication::translate("Core", "Add-on not found"));
        return;
    }

    const QString jobId = m_jobOrchestrator->startAddonDownload(*entry, *addon);
    if (jobId.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "Could not start add-on download"));
        return;
    }
}

void CoreController::updateCatalogEntry(const QString& entryId)
{
    const CatalogEntry* entry = findCatalogEntry(entryId);
    if (!entry) {
        showNotice(QCoreApplication::translate("Core", "Entry not found: %1").arg(entryId));
        return;
    }

    const LibraryGame* game = m_libraryStore.gameById(entryId);
    const QString libId = game && !game->libraryId.isEmpty() ? game->libraryId
                                                            : m_settings.defaultLibraryId();

    if (m_pluginHost && m_pluginHost->pluginOwnsDownload(entry->sourceId)) {
        installCatalogEntry(entryId, libId, {}, QStringLiteral("update"));
        return;
    }

    const QString jobId = m_jobOrchestrator->startCatalogDownload(*entry, JobKind::Update, libId);
    if (jobId.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "Could not start update for %1").arg(entry->title));
        return;
    }
}

void CoreController::prepareShutdown()
{
    if (m_applicationDataCleared)
        return;
    if (m_prepareShutdownDone)
        return;
    m_prepareShutdownDone = true;

    if (m_launchController)
        m_launchController->stopRunningGame();

    if (m_catalogValidateLoader)
        m_catalogValidateLoader->cancelActive();

    if (m_jobOrchestrator)
        m_jobOrchestrator->flushPersistence();
    if (m_httpSession)
        m_httpSession->shutdown();
    if (m_torrentSession)
        m_torrentSession->shutdown();
    if (m_pluginHost)
        m_pluginHost->shutdownPlugins();
}

} // namespace arachnel::core

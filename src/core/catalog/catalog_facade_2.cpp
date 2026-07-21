#include "core_controller_impl.h"

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

void CoreController::searchCatalog(const QString& sourceId, const QString& query)
{
    const SourcePluginInfo* source = m_sources.pluginById(sourceId);
    if (!source) {
        showNotice(QCoreApplication::translate("Core", "Unknown source: %1").arg(sourceId));
        return;
    }
    if (!source->enabled) {
        showNotice(QCoreApplication::translate("Core", "Source \"%1\" is disabled in settings").arg(source->name));
        return;
    }

    if (m_catalogController)
        m_catalogController->selectCatalogSource(sourceId, query);
}

void CoreController::setActiveCatalogSource(const QString& sourceId)
{
    if (m_catalogController)
        m_catalogController->setActiveCatalogSource(sourceId);
}

bool CoreController::isCatalogSourceSelected(const QString& sourceId) const
{
    return m_catalogController && m_catalogController->isCatalogSourceSelected(sourceId);
}

void CoreController::toggleCatalogSource(const QString& sourceId)
{
    if (m_catalogController)
        m_catalogController->toggleCatalogSource(sourceId);
}

void CoreController::applyCatalogSearch(const QString& query)
{
    if (m_catalogController)
        m_catalogController->applyCatalogSearch(query);
}

void CoreController::pruneDisabledCatalogSources()
{
    if (m_catalogController)
        m_catalogController->pruneDisabledCatalogSources();
}

void CoreController::selectCatalogSource(const QString& sourceId, const QString& query)
{
    if (m_catalogController)
        m_catalogController->selectCatalogSource(sourceId, query);
}

void CoreController::clearCatalogView()
{
    if (m_catalogController)
        m_catalogController->clearCatalogView();
}

int CoreController::catalogEntryCount(const QString& sourceId) const
{
    return m_catalogController ? m_catalogController->catalogEntryCount(sourceId) : -1;
}

void CoreController::invalidateSourceCatalog(const QString& sourceId)
{
    if (m_catalogController)
        m_catalogController->invalidateSourceCatalog(sourceId);
}

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
    if (m_runningGameTimer)
        m_runningGameTimer->stop();
    clearRunningGame();

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

void CoreController::prefetchCatalogCounts()
{
    if (m_catalogController)
        m_catalogController->prefetchCatalogCounts();
}

void CoreController::validateHydraCatalogUrl(const QString& requestId, const QString& url)
{
    const QString trimmed = url.trimmed();
    if (requestId.isEmpty() || trimmed.isEmpty()) {
        emit hydraCatalogUrlValidated(requestId, false, 0,
                                      QCoreApplication::translate("Core", "Enter a catalog URL"));
        return;
    }

    const QUrl parsed(trimmed);
    if (!parsed.isValid() || !parsed.scheme().startsWith(QStringLiteral("http"),
                                                         Qt::CaseInsensitive)) {
        emit hydraCatalogUrlValidated(requestId, false, 0,
                                      QCoreApplication::translate("Core", "Invalid URL — http or https required"));
        return;
    }

    m_catalogValidateLoader->loadFeed(parsed, QStringLiteral("validate:%1").arg(requestId));
}

void CoreController::installCatalogEntry(const QString& entryId, const QString& libraryId,
                                         const QVariantList& addonIdsVariant)
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

        const QString jobId =
            m_jobOrchestrator->startPluginOwnedDownload(entry, JobKind::Download, libId);
        if (jobId.isEmpty()) {
            showNotice(
                QCoreApplication::translate("Core", "Could not start download for %1").arg(entry.title));
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

        InstallContext ctx;
        ctx.jobId = jobId;
        ctx.entryId = entry.id;
        ctx.sourceId = entry.sourceId;
        ctx.title = entry.title;
        ctx.targetPath = m_settings.resolvedLibraryRoot(libId) + QLatin1Char('/') + entry.id;
        ctx.downloadsPath = m_settings.resolvedDownloadsRoot(libId);
        ctx.downloadPath = ctx.downloadsPath + QLatin1Char('/') + QStringLiteral("install/") + entry.id;
        ctx.magnetUri = entry.steamAppId;
        ctx.uploadDate = entry.uploadDate;
        ctx.installKind = entry.installKind;

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
        installCatalogEntry(entryId, libId, {});
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

    if (m_runningGameTimer)
        m_runningGameTimer->stop();
    clearRunningGame();

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
    qmlRegisterUncreatableType<NotificationModel>("Arachnel.Core", 1, 0, "NotificationModel",
                                                  QStringLiteral("Use Core.notifications"));
    qmlRegisterUncreatableType<SettingsStore>("Arachnel.Core", 1, 0, "SettingsStore",
                                              QStringLiteral("Use Core.settings"));
    qmlRegisterUncreatableType<AppUpdater>("Arachnel.Core", 1, 0, "AppUpdater",
                                           QStringLiteral("Use Core.appUpdater"));
    qmlRegisterUncreatableType<PluginCatalogService>("Arachnel.Core", 1, 0, "PluginCatalogService",
                                                     QStringLiteral("Use Core.pluginCatalog"));
    qmlRegisterUncreatableType<StorageLibraryModel>("Arachnel.Core", 1, 0, "StorageLibraryModel",
                                                    QStringLiteral("Use Core.settings.storageLibraries"));
}

} // namespace arachnel::core

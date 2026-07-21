#include "core_controller_impl.h"

namespace arachnel::core {

void CoreController::advanceInstallSession(const QString& entryId)
{
    m_installSessionService->advanceInstallSession(entryId);
}

void CoreController::commitInstalledCatalogGame(const CatalogEntry& entryHint,
                                                const QString& sourceId, const QString& savePath,
                                                const QString& libraryId, const QString& installPath,
                                                InstallKind installKind)
{
    m_installSessionService->commitInstalledCatalogGame(entryHint, sourceId, savePath, libraryId,
                                                        installPath, installKind);
}

void CoreController::markCatalogAddonInstalled(const QString& parentEntryId,
                                               const QString& addonId, const QString& uploadDate)
{
    const LibraryGame* existing = m_libraryStore.gameById(parentEntryId);
    if (!existing)
        return;

    QString resolvedUploadDate = uploadDate;
    if (const CatalogEntry* parent = findCatalogEntry(parentEntryId)) {
        if (const CatalogComponent* remoteAddon = findCatalogAddon(*parent, addonId)) {
            if (!remoteAddon->uploadDate.isEmpty())
                resolvedUploadDate = remoteAddon->uploadDate;
        }
    }

    LibraryGame game = *existing;
    bool found = false;
    for (auto& component : game.components) {
        if (component.id != addonId)
            continue;
        component.installed = true;
        if (!resolvedUploadDate.isEmpty())
            component.uploadDate = resolvedUploadDate;
        found = true;
        break;
    }

    if (!found) {
        InstalledComponent component;
        component.id = addonId;
        component.installed = true;
        component.uploadDate = resolvedUploadDate;
        game.components.append(component);
    }

    m_libraryStore.upsertGame(game);
    syncLibraryFromStore();
    if (!m_catalogCache.isEmpty())
        recalculateLibraryUpdates(false);
}

QString CoreController::resolveAddonArtifactPath(const QString& parentEntryId,
                                                 const QString& addonId) const
{
    const JobEntry* latest = nullptr;
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (job.entryId != addonId || job.parentEntryId != parentEntryId)
            continue;
        if (job.status != QStringLiteral("completed"))
            continue;
        if (!latest || job.completedAt > latest->completedAt)
            latest = &job;
    }
    if (!latest)
        return {};

    if (!latest->artifactPath.isEmpty() && QFileInfo::exists(latest->artifactPath))
        return latest->artifactPath;

    if (!latest->savePath.isEmpty() && QFileInfo::exists(latest->savePath))
        return latest->savePath;

    return {};
}

bool CoreController::isCatalogAddonInstalled(const QString& entryId,
                                             const QString& addonId) const
{
    const LibraryGame* game = m_libraryStore.gameById(entryId);
    if (!game)
        return false;
    for (const auto& component : game->components) {
        if (component.id == addonId)
            return component.installed;
    }
    return false;
}

void CoreController::installDownloadedCatalogAddon(const QString& entryId, const QString& addonId)
{
    const CatalogEntry* parent = findCatalogEntry(entryId);
    if (!parent) {
        showNotice(QCoreApplication::translate("Core", "Game not found"));
        return;
    }

    const CatalogComponent* addon = findCatalogAddon(*parent, addonId);
    if (!addon) {
        showNotice(QCoreApplication::translate("Core", "Add-on not found"));
        return;
    }

    const QString artifactPath = resolveAddonArtifactPath(entryId, addonId);
    if (artifactPath.isEmpty()) {
        showNotice(QCoreApplication::translate("Core", "Download the add-on first"));
        return;
    }

    const QVariantMap jobMap = m_jobs.jobForAddon(entryId, addonId);
    const QString jobId = jobMap.value(QStringLiteral("jobId")).toString();
    startPluginAddonInstall(*parent, *addon, parent->sourceId, artifactPath, jobId);
}

std::optional<CatalogEntry> CoreController::resolveCatalogEntry(const QString& entryId,
                                                                const QString& sourceId,
                                                                const JobEntry* jobHint) const
{
    if (const CatalogEntry* cached = findCatalogEntry(entryId))
        return *cached;

    if (ISourcePlugin* plugin = m_pluginHost->plugin(sourceId)) {
        if (const auto remote = plugin->entryById(entryId))
            return *remote;
    }

    if (!jobHint)
        return std::nullopt;

    CatalogEntry synthetic;
    synthetic.id = entryId;
    synthetic.sourceId = sourceId;
    synthetic.title = jobHint->title;
    if (synthetic.title.startsWith(QStringLiteral("Downloading ")))
        synthetic.title = synthetic.title.mid(12);
    else if (synthetic.title.startsWith(QStringLiteral("Загрузка ")))
        synthetic.title = synthetic.title.mid(9);
    else if (synthetic.title.startsWith(QStringLiteral("Updating ")))
        synthetic.title = synthetic.title.mid(9);
    else if (synthetic.title.startsWith(QStringLiteral("Обновление ")))
        synthetic.title = synthetic.title.mid(11);
    if (!jobHint->magnetUri.isEmpty())
        synthetic.magnetUris.append(jobHint->magnetUri);
    synthetic.coverUrl = jobHint->coverUrl;
    return synthetic;
}

bool CoreController::gameNeedsInstall(const QString& entryId) const
{
    const LibraryGame* game = m_libraryStore.gameById(entryId);
    if (!game || game->installPath.isEmpty())
        return true;
    return !QFileInfo::exists(game->installPath);
}

const JobEntry* CoreController::findLatestJobForEntry(const QString& entryId) const
{
    const JobEntry* latest = nullptr;
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (job.entryId != entryId)
            continue;
        if (!latest || job.completedAt > latest->completedAt || job.createdAt > latest->createdAt)
            latest = &job;
    }
    return latest;
}

bool CoreController::isEntryDownloadComplete(const QString& entryId) const
{
    return m_libraryController && m_libraryController->isEntryDownloadComplete(entryId);
}

bool CoreController::entryDownloadFilesExist(const QString& entryId) const
{
    return m_libraryController && m_libraryController->entryDownloadFilesExist(entryId);
}

QVariantMap CoreController::entryDetails(const QString& entryId) const
{
    return m_libraryController ? m_libraryController->entryDetails(entryId) : QVariantMap();
}

void CoreController::reconcileJobInstallState()
{
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (!job.parentEntryId.isEmpty()) {
            if (job.status != QStringLiteral("installing"))
                continue;
            if (isCatalogAddonInstalled(job.parentEntryId, job.entryId)) {
                m_jobOrchestrator->setJobPhase(job.id, QStringLiteral("completed"),
                                               QStringLiteral("Installed"));
                continue;
            }
            if (!resolveAddonArtifactPath(job.parentEntryId, job.entryId).isEmpty()) {
                m_jobOrchestrator->setJobPhase(job.id, QStringLiteral("completed"),
                                               QStringLiteral("Download complete"));
            }
            continue;
        }
        if (job.status != QStringLiteral("completed"))
            continue;
        if (!gameNeedsInstall(job.entryId)) {
            if (job.detail == QStringLiteral("Installation required")
                || job.detail == QStringLiteral("Требуется установка")) {
                m_jobOrchestrator->setJobPhase(job.id, QStringLiteral("completed"),
                                               QStringLiteral("Installed"));
            }
            continue;
        }
        if (job.detail == QStringLiteral("Installed")
            || job.detail == QStringLiteral("Установлено")) {
            m_jobOrchestrator->setJobPhase(job.id, QStringLiteral("completed"),
                                           QStringLiteral("Installation required"));
        }
    }
}

void CoreController::pruneUnselectedAddonJobs(const QString& parentEntryId,
                                              const QStringList& selectedAddonIds)
{
    if (parentEntryId.isEmpty())
        return;

    const QSet<QString> keep(selectedAddonIds.cbegin(), selectedAddonIds.cend());
    QVector<QString> removeIds;
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (job.parentEntryId != parentEntryId)
            continue;
        if (keep.contains(job.entryId))
            continue;
        removeIds.append(job.id);
    }

    for (const QString& jobId : removeIds)
        m_jobOrchestrator->removeJob(jobId);
}

void CoreController::pruneCancelledAddonJobs()
{
    QVector<QString> removeIds;
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (job.parentEntryId.isEmpty())
            continue;
        if (job.status == QStringLiteral("cancelled"))
            removeIds.append(job.id);
    }

    for (const QString& jobId : removeIds)
        m_jobOrchestrator->removeJob(jobId);
}

void CoreController::removeJobsForEntry(const QString& entryId)
{
    QVector<QString> jobIds;
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (job.entryId == entryId)
            jobIds.append(job.id);
    }
    for (const QString& jobId : jobIds)
        m_jobOrchestrator->removeJob(jobId);
}

void CoreController::retryPendingInstalls()
{
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (!job.parentEntryId.isEmpty())
            continue;
        if (job.status != QStringLiteral("completed"))
            continue;
        if (job.kind != JobKind::Download && job.kind != JobKind::Update)
            continue;
        if (!gameNeedsInstall(job.entryId))
            continue;
        if (job.savePath.isEmpty() || !QDir(job.savePath).exists())
            continue;
        if (!hasInstallHandlerForPath(job.sourceId, job.savePath))
            continue;

        const auto entry = resolveCatalogEntry(job.entryId, job.sourceId, &job);
        if (!entry)
            continue;

        startPluginInstall(*entry, job.sourceId, job.savePath, job.kind, job.libraryId, job.id);
    }
}

bool CoreController::entryHasActiveJob(const QString& entryId) const
{
    for (const JobEntry& job : m_jobStore.jobs()) {
        if (job.entryId != entryId && job.parentEntryId != entryId)
            continue;
        if (isJobInProgress(job.status))
            return true;
    }
    return false;
}

void CoreController::cancelJob(const QString& jobId)
{
    const JobEntry* job = m_jobStore.jobById(jobId);
    if (job && !job->parentEntryId.isEmpty()) {
        m_jobOrchestrator->removeJob(jobId);
        return;
    }

    if (job && job->pluginDownload && m_pluginHost)
        m_pluginHost->cancelOwnedDownload(job->sourceId, jobId);

    const QString entryId = job ? job->entryId : QString();
    m_jobOrchestrator->cancelJob(jobId);

    if (entryId.isEmpty())
        return;

    m_installSessionService->cancelEntry(entryId);

    QVector<QString> staleJobIds;
    for (const JobEntry& stored : m_jobStore.jobs()) {
        if (stored.entryId != entryId || !stored.parentEntryId.isEmpty())
            continue;
        if (stored.status == QStringLiteral("cancelled")
            || stored.status == QStringLiteral("failed"))
            staleJobIds.append(stored.id);
    }
    for (const QString& staleId : staleJobIds)
        m_jobOrchestrator->removeJob(staleId);
}

void CoreController::toggleJobPause(const QString& jobId)
{
    if (jobId.isEmpty())
        return;
    m_jobOrchestrator->toggleJobPause(jobId);
}

void CoreController::removeJob(const QString& jobId)
{
    if (jobId.isEmpty())
        return;
    m_jobOrchestrator->removeJob(jobId);
}

void CoreController::retryJob(const QString& jobId)
{
    if (jobId.isEmpty())
        return;
    m_jobOrchestrator->retryJob(jobId);
}

} // namespace arachnel::core

#include "core_controller_impl.h"

namespace arachnel::core {

void CoreController::retryInstall(const QString& jobId)
{
    if (jobId.isEmpty())
        return;

    const JobEntry* job = m_jobStore.jobById(jobId);
    if (!job) {
        showNotice(QCoreApplication::translate("Core", "Download not found"));
        return;
    }
    if (job->status != QStringLiteral("completed")) {
        showNotice(QCoreApplication::translate("Core", "Installation is only available for completed downloads"));
        return;
    }

    if (!job->parentEntryId.isEmpty()) {
        installDownloadedCatalogAddon(job->parentEntryId, job->entryId);
        return;
    }

    if (!gameNeedsInstall(job->entryId)) {
        for (const JobEntry& addonJob : m_jobStore.jobs()) {
            if (addonJob.parentEntryId != job->entryId)
                continue;
            if (isCatalogAddonInstalled(job->entryId, addonJob.entryId))
                continue;
            if (resolveAddonArtifactPath(job->entryId, addonJob.entryId).isEmpty())
                continue;
            installDownloadedCatalogAddon(job->entryId, addonJob.entryId);
            return;
        }

        const CatalogEntry* parent = findCatalogEntry(job->entryId);
        if (parent) {
            for (const auto& addon : parent->addons) {
                if (isCatalogAddonInstalled(job->entryId, addon.id))
                    continue;
                if (!resolveAddonArtifactPath(job->entryId, addon.id).isEmpty()) {
                    installDownloadedCatalogAddon(job->entryId, addon.id);
                    return;
                }
            }

            bool pendingAddon = false;
            for (const auto& addon : parent->addons) {
                if (!isCatalogAddonInstalled(job->entryId, addon.id)) {
                    pendingAddon = true;
                    break;
                }
            }
            if (pendingAddon) {
                showNotice(QCoreApplication::translate("Core", "Add-on file not found"));
                return;
            }
        }

        if (isEntryPlayable(job->entryId))
            return;

        // Install folder exists but launch is not ready — allow base game reinstall.
    }

    if (job->savePath.isEmpty() || !QDir(job->savePath).exists()) {
        showNotice(QCoreApplication::translate("Core", "Download files not found"));
        return;
    }

    const auto entry = resolveCatalogEntry(job->entryId, job->sourceId, job);
    if (!entry) {
        showNotice(QCoreApplication::translate("Core", "Could not find game to install"));
        return;
    }

    if (!hasInstallHandlerForPath(job->sourceId, job->savePath)) {
        offerManualInstallForJob(*job);
        return;
    }

    startPluginInstall(*entry, job->sourceId, job->savePath, job->kind, job->libraryId, job->id);
}

bool CoreController::canRetryJobInstall(const QString& jobId) const
{
    const JobEntry* job = m_jobStore.jobById(jobId);
    if (!job || job->status != QStringLiteral("completed"))
        return false;

    if (isJobInstallFailed(job->detail)) {
        if (!job->parentEntryId.isEmpty()) {
            if (isCatalogAddonInstalled(job->parentEntryId, job->entryId))
                return false;
            return !resolveAddonArtifactPath(job->parentEntryId, job->entryId).isEmpty();
        }
        if (gameNeedsInstall(job->entryId))
            return !job->savePath.isEmpty() && QDir(job->savePath).exists();
        for (const JobEntry& addonJob : m_jobStore.jobs()) {
            if (addonJob.parentEntryId != job->entryId)
                continue;
            if (isCatalogAddonInstalled(job->entryId, addonJob.entryId))
                continue;
            if (!resolveAddonArtifactPath(job->entryId, addonJob.entryId).isEmpty())
                return true;
        }
        return false;
    }

    if (!job->parentEntryId.isEmpty()) {
        if (isCatalogAddonInstalled(job->parentEntryId, job->entryId))
            return false;
        return !resolveAddonArtifactPath(job->parentEntryId, job->entryId).isEmpty();
    }

    if (gameNeedsInstall(job->entryId))
        return !job->savePath.isEmpty() && QDir(job->savePath).exists();

    const CatalogEntry* parent = findCatalogEntry(job->entryId);
    if (!parent)
        return false;
    for (const auto& addon : parent->addons) {
        if (isCatalogAddonInstalled(job->entryId, addon.id))
            continue;
        if (!resolveAddonArtifactPath(job->entryId, addon.id).isEmpty())
            return true;
    }
    return false;
}

bool CoreController::canManualInstallJob(const QString& jobId) const
{
    const JobEntry* job = m_jobStore.jobById(jobId);
    if (!job || job->status != QStringLiteral("completed"))
        return false;
    if (!job->parentEntryId.isEmpty())
        return false;
    if (!gameNeedsInstall(job->entryId))
        return false;
    return !job->savePath.isEmpty() && QDir(job->savePath).exists();
}

void CoreController::openJobDownloadFolder(const QString& jobId)
{
    const JobEntry* job = m_jobStore.jobById(jobId);
    if (!job || job->savePath.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(job->savePath));
}

QString CoreController::browseInstallFolder(const QString& startPath)
{
#if defined(Q_OS_WIN)
    QString path;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool comOwned = SUCCEEDED(hr);

    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&dialog)))) {
        DWORD options = 0;
        if (SUCCEEDED(dialog->GetOptions(&options)))
            dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        dialog->SetTitle(L"Choose game install folder");

        if (!startPath.isEmpty()) {
            IShellItem* folder = nullptr;
            QString folderPath = QFileInfo(startPath).isDir() ? startPath
                                                              : QFileInfo(startPath).absolutePath();
            folderPath = QDir::toNativeSeparators(folderPath);
            if (QDir(folderPath).exists()
                && SUCCEEDED(SHCreateItemFromParsingName(
                    reinterpret_cast<LPCWSTR>(folderPath.utf16()), nullptr,
                    IID_PPV_ARGS(&folder)))) {
                dialog->SetFolder(folder);
                folder->Release();
            }
        }

        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR widePath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &widePath))) {
                    path = QString::fromWCharArray(widePath);
                    CoTaskMemFree(widePath);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (comOwned)
        CoUninitialize();
    return path;
#else
    return QFileDialog::getExistingDirectory(
        nullptr, QCoreApplication::translate("Core", "Choose game install folder"), startPath);
#endif
}

void CoreController::offerManualInstallForJob(const JobEntry& job)
{
    openJobDownloadFolder(job.id);
    showNotice(QCoreApplication::translate(
        "Core",
        "Automatic install is unavailable. Run setup.exe from the download folder, then use the folder button to point to the game."));
}

void CoreController::confirmManualInstall(const QString& jobId)
{
    const JobEntry* job = m_jobStore.jobById(jobId);
    if (!job)
        return;

    if (job->savePath.isEmpty() || !QDir(job->savePath).exists()) {
        showNotice(QCoreApplication::translate("Core", "Download files not found"));
        return;
    }

    const QString installFolder = browseInstallFolder(job->savePath);
    if (installFolder.isEmpty())
        return;

    const QString executable = findGameExecutableInTree(installFolder);
    if (executable.isEmpty()) {
        showNotice(QCoreApplication::translate(
            "Core", "No game executable found in %1").arg(installFolder));
        return;
    }

    const auto entry = resolveCatalogEntry(job->entryId, job->sourceId, job);
    if (!entry) {
        showNotice(QCoreApplication::translate("Core", "Could not find game to install"));
        return;
    }

    const InstallKind kind = detectInstallKindForEntry(job->sourceId, job->savePath);
    commitInstalledCatalogGame(*entry, job->sourceId, job->savePath, job->libraryId, installFolder,
                               kind);
    setGameExecutableOverride(job->entryId, executable);
    m_jobOrchestrator->setJobPhase(jobId, QStringLiteral("completed"),
                                   QCoreApplication::translate("Core", "Installed"));
    showNotice(QCoreApplication::translate("Core", "Manual install complete for %1").arg(entry->title));
}

void CoreController::clearFinishedJobs()
{
    m_jobOrchestrator->clearFinishedJobs();
}

} // namespace arachnel::core

#include "install_session_service.h"

#include "install_analyzer.h"
#include "job_model.h"
#include "job_orchestrator.h"
#include "job_store.h"
#include "plugin_host.h"
#include "proton_manager.h"
#include "settings_store.h"

#include <QCoreApplication>
#include <QDir>

namespace arachnel::core {

InstallSessionService::InstallSessionService(
    SettingsStore* settings, LibraryStore* libraryStore, JobStore* jobStore, JobModel* jobs,
    JobOrchestrator* jobOrchestrator, PluginHost* pluginHost, InstallAnalyzer* installAnalyzer,
    ProtonManager* protonManager, Hooks hooks, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_libraryStore(libraryStore)
    , m_jobStore(jobStore)
    , m_jobs(jobs)
    , m_jobOrchestrator(jobOrchestrator)
    , m_pluginHost(pluginHost)
    , m_installAnalyzer(installAnalyzer)
    , m_protonManager(protonManager)
    , m_hooks(std::move(hooks))
{
}

void InstallSessionService::startPluginInstall(const CatalogEntry& entry, const QString& sourceId,
                                               const QString& savePath, JobKind kind,
                                               const QString& libraryId, const QString& jobId)
{
    if (m_installingEntries.contains(entry.id)) {
        m_hooks.showNotice(QCoreApplication::translate("Core", "Installation of %1 is already in progress")
                               .arg(entry.title),
                           true);
        return;
    }

    const QString libId = libraryId.isEmpty() ? m_settings->defaultLibraryId() : libraryId;
    InstallContext ctx;
    ctx.jobId = jobId;
    ctx.entryId = entry.id;
    ctx.sourceId = sourceId;
    ctx.title = entry.title;
    const LibraryGame* existing = m_libraryStore->gameById(entry.id);
    ctx.targetPath = existing && !existing->installPath.isEmpty() && QDir(existing->installPath).exists()
                         ? existing->installPath
                         : m_settings->gameDirFor(libId, entry.id);
    ctx.downloadsPath = m_settings->resolvedDownloadsRoot(libId);
    ctx.downloadPath = savePath;
    ctx.magnetUri = entry.magnetUris.value(0);
    ctx.uploadDate = entry.uploadDate;
    m_hooks.fillProtonInstallFields(
        ctx.entryId, m_settings->resolvedProtonId(QString(), *m_protonManager),
        &ctx.protonExecutable, &ctx.compatDataPath, &ctx.steamCompatClientPath);

    const InstallPlan plan = m_installAnalyzer ? m_installAnalyzer->resolveDownload(ctx) : InstallPlan{};
    if (!plan.installerPlugin) {
        if (!jobId.isEmpty()) {
            if (const JobEntry* job = m_jobStore->jobById(jobId))
                m_hooks.offerManualInstall(*job);
        } else {
            m_hooks.showNotice(
                QCoreApplication::translate("Core", "Can't install %1 — install a plugin for this source")
                    .arg(entry.title),
                true);
        }
        return;
    }

    m_installingEntries.insert(entry.id);
    const InstallKind detectedKind = plan.analysis.kind;
    m_hooks.syncCatalogInstallKind(entry.id, detectedKind);
    ctx.installKind = detectedKind;
    if (!jobId.isEmpty()) {
        if (m_installSessions.contains(entry.id))
            syncInstallSessionPhase(entry.id);
        else
            m_jobOrchestrator->setJobPhase(jobId, QStringLiteral("installing"),
                                           QStringLiteral("Installing…"));
    }

    m_pluginHost->runInstallAsync(
        plan.installerPlugin, ctx,
        [this, entry, sourceId, savePath, kind, libId, jobId, detectedKind](const InstallResult& result) {
            m_installingEntries.remove(entry.id);
            if (!result.success) {
                const QString detail = result.error.isEmpty()
                                           ? QStringLiteral("Install failed")
                                           : QStringLiteral("Install failed: %1").arg(result.error);
                if (!jobId.isEmpty())
                    m_jobOrchestrator->setJobPhase(jobId, QStringLiteral("completed"), detail);
                clearSession(entry.id);
                m_hooks.showNotice(QCoreApplication::translate("Core", "Install failed for %1: %2")
                                       .arg(entry.title, result.error),
                                   true);
                if (!jobId.isEmpty()) {
                    if (const JobEntry* job = m_jobStore->jobById(jobId))
                        m_hooks.offerManualInstall(*job);
                }
                return;
            }

            commitInstalledCatalogGame(entry, sourceId, savePath, libId, result.installPath, detectedKind);
            if (GameInstallSession* session = m_installSessions.contains(entry.id)
                                                  ? &m_installSessions[entry.id]
                                                  : nullptr) {
                session->gameInstallDone = true;
                session->installStep = 1;
                syncInstallSessionPhase(entry.id);
                advanceInstallSession(entry.id);
            } else {
                for (const auto& addon : entry.addons) {
                    if (!m_hooks.isAddonInstalled(entry.id, addon.id)) {
                        const QString artifactPath = m_hooks.addonArtifactPath(entry.id, addon.id);
                        if (!artifactPath.isEmpty())
                            startPluginAddonInstall(entry, addon, sourceId, artifactPath, jobId);
                    }
                }
                if (!jobId.isEmpty())
                    m_jobOrchestrator->setJobPhase(jobId, QStringLiteral("completed"),
                                                   QStringLiteral("Installed"));
            }
            m_hooks.reconcileJobInstallState();
            m_hooks.showNotice(kind == JobKind::Update
                                   ? QCoreApplication::translate("Core", "Update installed: %1").arg(entry.title)
                                   : QCoreApplication::translate("Core", "Installed: %1").arg(entry.title),
                               true);
        });
}

void InstallSessionService::beginInstallSession(const QString& entryId, const QString& gameJobId,
                                                const QString& sourceId, const QStringList& addonIds)
{
    m_installSessions.insert(
        entryId, {gameJobId, sourceId, addonIds, 1, 1 + static_cast<int>(addonIds.size()), false});
    m_installSelectedAddons.insert(entryId, addonIds);
}

void InstallSessionService::syncInstallSessionPhase(const QString& entryId)
{
    const auto it = m_installSessions.constFind(entryId);
    if (it == m_installSessions.cend() || it->gameJobId.isEmpty())
        return;
    m_jobOrchestrator->setJobPhase(
        it->gameJobId, QStringLiteral("installing"),
        QStringLiteral("Installing (%1/%2)").arg(qMax(1, it->installStep)).arg(qMax(1, it->installTotal)));
}

void InstallSessionService::advanceInstallSession(const QString& entryId)
{
    auto it = m_installSessions.find(entryId);
    if (it == m_installSessions.end() || !it->gameInstallDone)
        return;
    const CatalogEntry* parent = m_hooks.findCatalogEntry(entryId);
    if (!parent || !m_hooks.isEntryPlayable(entryId))
        return;

    int installedCount = 1;
    for (const QString& addonId : it->selectedAddonIds) {
        if (m_hooks.isAddonInstalled(entryId, addonId))
            ++installedCount;
    }
    for (const QString& addonId : it->selectedAddonIds) {
        if (m_hooks.isAddonInstalled(entryId, addonId))
            continue;
        const QString artifactPath = m_hooks.addonArtifactPath(entryId, addonId);
        if (artifactPath.isEmpty())
            return;
        const CatalogComponent* addon = m_hooks.findCatalogAddon(*parent, addonId);
        if (!addon)
            continue;
        it->installStep = installedCount + 1;
        syncInstallSessionPhase(entryId);
        startPluginAddonInstall(*parent, *addon, it->sourceId, artifactPath, it->gameJobId,
                                [this, entryId](bool success) {
                                    if (!success)
                                        clearSession(entryId);
                                    else
                                        advanceInstallSession(entryId);
                                });
        return;
    }
    m_jobOrchestrator->setJobPhase(it->gameJobId, QStringLiteral("completed"),
                                   QStringLiteral("Installed"));
    clearSession(entryId);
    m_hooks.reconcileJobInstallState();
}

void InstallSessionService::completePluginDownload(const CatalogEntry& entry, const QString& sourceId,
                                                   const QString& savePath, const QString& libraryId,
                                                   const QString& artifactPath, const QString& jobId)
{
    commitInstalledCatalogGame(entry, sourceId, savePath, libraryId, artifactPath, entry.installKind);
    if (GameInstallSession* session = m_installSessions.contains(entry.id) ? &m_installSessions[entry.id]
                                                                             : nullptr) {
        session->gameInstallDone = true;
        session->installStep = 1;
        syncInstallSessionPhase(entry.id);
        advanceInstallSession(entry.id);
    } else {
        m_jobOrchestrator->setJobPhase(jobId, QStringLiteral("completed"), QStringLiteral("Installed"));
    }
}

void InstallSessionService::cancelEntry(const QString& entryId)
{
    m_installingEntries.remove(entryId);
    clearSession(entryId);
}

void InstallSessionService::clearSession(const QString& entryId)
{
    m_installSessions.remove(entryId);
    m_installSelectedAddons.remove(entryId);
}

} // namespace arachnel::core

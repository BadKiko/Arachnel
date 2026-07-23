#pragma once

#include "catalog_types.h"
#include "install_analysis.h"
#include "job_kind.h"
#include "library_store.h"

#include <QObject>
#include <QHash>
#include <QSet>
#include <functional>

namespace arachnel::core {

struct JobEntry;

class InstallAnalyzer;
class JobModel;
class JobOrchestrator;
class JobStore;
class PluginHost;
class ProtonManager;
class SettingsStore;

class InstallSessionService : public QObject
{
    Q_OBJECT

public:
    struct Hooks {
        std::function<void(const QString&, bool)> showNotice;
        std::function<const CatalogEntry*(const QString&)> findCatalogEntry;
        std::function<const CatalogComponent*(const CatalogEntry&, const QString&)> findCatalogAddon;
        std::function<bool(const QString&)> isEntryPlayable;
        std::function<bool(const QString&, const QString&)> isAddonInstalled;
        std::function<QString(const QString&, const QString&)> addonArtifactPath;
        std::function<void(const QString&, const QString&, const QString&)> markAddonInstalled;
        std::function<void(const QString&, InstallKind)> syncCatalogInstallKind;
        std::function<void(const JobEntry&)> offerManualInstall;
        std::function<void()> reconcileJobInstallState;
        std::function<void()> syncLibrary;
        std::function<void()> recalculateLibraryUpdates;
        std::function<QString(const QString&)> sourceNameForId;
        std::function<QString(const QString&)> metadataSteamAppIdForTitle;
        std::function<QString(const QString&)> findGameExecutable;
        std::function<void(const QString&, const QString&, QString*, QString*, QString*)>
            fillProtonInstallFields;
        std::function<void(const LibraryGame&)> gameCommitted;
    };

    InstallSessionService(SettingsStore* settings, LibraryStore* libraryStore, JobStore* jobStore,
                          JobModel* jobs, JobOrchestrator* jobOrchestrator,
                          PluginHost* pluginHost, InstallAnalyzer* installAnalyzer,
                          ProtonManager* protonManager, Hooks hooks, QObject* parent = nullptr);

    void startPluginInstall(const CatalogEntry& entry, const QString& sourceId,
                            const QString& savePath, JobKind kind, const QString& libraryId = {},
                            const QString& jobId = {});
    void startPluginAddonInstall(const CatalogEntry& parent, const CatalogComponent& addon,
                                 const QString& sourceId, const QString& artifactPath,
                                 const QString& progressJobId = {},
                                 std::function<void(bool)> done = {});
    void beginInstallSession(const QString& entryId, const QString& gameJobId,
                             const QString& sourceId, const QStringList& addonIds);
    void advanceInstallSession(const QString& entryId);
    void completePluginDownload(const CatalogEntry& entry, const QString& sourceId,
                                const QString& savePath, const QString& libraryId,
                                const QString& artifactPath, const QString& jobId);
    void commitInstalledCatalogGame(const CatalogEntry& entryHint, const QString& sourceId,
                                    const QString& savePath, const QString& libraryId,
                                    const QString& installPath, InstallKind installKind);
    void cancelEntry(const QString& entryId);

private:
    struct GameInstallSession {
        QString gameJobId;
        QString sourceId;
        QStringList selectedAddonIds;
        int installStep = 0;
        int installTotal = 1;
        bool gameInstallDone = false;
    };

    void syncInstallSessionPhase(const QString& entryId);
    void clearSession(const QString& entryId);
    /** Delete torrent/installer payload under downloads after a successful install. */
    void cleanupDownloadArtifact(const QString& artifactPath, const QString& installPath,
                                 const QString& libraryId) const;

    SettingsStore* m_settings = nullptr;
    LibraryStore* m_libraryStore = nullptr;
    JobStore* m_jobStore = nullptr;
    JobModel* m_jobs = nullptr;
    JobOrchestrator* m_jobOrchestrator = nullptr;
    PluginHost* m_pluginHost = nullptr;
    InstallAnalyzer* m_installAnalyzer = nullptr;
    ProtonManager* m_protonManager = nullptr;
    Hooks m_hooks;
    QSet<QString> m_installingEntries;
    QSet<QString> m_installingAddons;
    QHash<QString, QStringList> m_installSelectedAddons;
    QHash<QString, GameInstallSession> m_installSessions;
};

} // namespace arachnel::core

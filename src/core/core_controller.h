#pragma once

#include "catalog_model.h"
#include "job_model.h"
#include "library_store.h"
#include "library_model.h"
#include "settings_store.h"
#include "source_plugin_model.h"

#include <QObject>
#include <QVector>

class QQmlEngine;
class QJSEngine;

namespace arachnel::core {

class CatalogFeedLoader;
class GameMetadataService;
class JobOrchestrator;
class TorrentSession;

class CoreController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(LibraryModel* library READ library CONSTANT)
    Q_PROPERTY(SourcePluginModel* sources READ sources CONSTANT)
    Q_PROPERTY(CatalogModel* catalog READ catalog CONSTANT)
    Q_PROPERTY(JobModel* jobs READ jobs CONSTANT)
    Q_PROPERTY(SettingsStore* settings READ settings CONSTANT)
    Q_PROPERTY(QString lastAction READ lastAction NOTIFY lastActionChanged)
    Q_PROPERTY(bool catalogLoading READ catalogLoading NOTIFY catalogLoadingChanged)
    Q_PROPERTY(QString catalogStatus READ catalogStatus NOTIFY catalogStatusChanged)

public:
    static CoreController* create(QQmlEngine* engine, QJSEngine* scriptEngine);
    static CoreController& instance();

    LibraryModel* library() { return &m_library; }
    SourcePluginModel* sources() { return &m_sources; }
    CatalogModel* catalog() { return &m_catalog; }
    JobModel* jobs() { return &m_jobs; }
    SettingsStore* settings() { return &m_settings; }
    QString lastAction() const { return m_lastAction; }
    bool catalogLoading() const { return m_catalogLoading; }
    QString catalogStatus() const { return m_catalogStatus; }

    Q_INVOKABLE void launchGame(const QString& gameId);
    Q_INVOKABLE void searchCatalog(const QString& sourceId, const QString& query);
    Q_INVOKABLE void installCatalogEntry(const QString& entryId);
    Q_INVOKABLE void installCatalogAddon(const QString& entryId, const QString& addonId);
    Q_INVOKABLE void updateCatalogEntry(const QString& entryId);
    Q_INVOKABLE void checkUpdates();
    Q_INVOKABLE void cancelJob(const QString& jobId);
    Q_INVOKABLE void refreshCatalog(const QString& sourceId);

signals:
    void lastActionChanged();
    void catalogLoadingChanged();
    void catalogStatusChanged();

private:
    explicit CoreController(QObject* parent = nullptr);

    void initializeServices();
    void loadSources();
    void syncLibraryFromStore();
    void applyCatalogFilter(const QString& sourceId, const QString& query);
    void setLastAction(const QString& action);
    void setCatalogLoading(bool loading);
    void setCatalogStatus(const QString& status);
    bool isRemoteUploadDateNewer(const QString& remote, const QString& local) const;
    const CatalogEntry* findCatalogEntry(const QString& entryId) const;
    const CatalogComponent* findCatalogAddon(const CatalogEntry& entry,
                                             const QString& addonId) const;

    LibraryModel m_library;
    SourcePluginModel m_sources;
    CatalogModel m_catalog;
    JobModel m_jobs;
    SettingsStore m_settings;
    LibraryStore m_libraryStore;
    CatalogFeedLoader* m_catalogLoader = nullptr;
    GameMetadataService* m_metadataService = nullptr;
    TorrentSession* m_torrentSession = nullptr;
    JobOrchestrator* m_jobOrchestrator = nullptr;

    QVector<CatalogEntry> m_catalogCache;
    QString m_activeSourceId;
    QString m_activeQuery;
    QString m_lastAction;
    QString m_catalogStatus;
    bool m_catalogLoading = false;
};

void registerCoreTypes();

} // namespace arachnel::core

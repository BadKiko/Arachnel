#pragma once

#include "catalog_model.h"
#include "job_model.h"
#include "library_model.h"
#include "source_plugin_model.h"

#include <QObject>

class QQmlEngine;
class QJSEngine;

namespace arachnel::core {

class CoreController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(LibraryModel* library READ library CONSTANT)
    Q_PROPERTY(SourcePluginModel* sources READ sources CONSTANT)
    Q_PROPERTY(CatalogModel* catalog READ catalog CONSTANT)
    Q_PROPERTY(JobModel* jobs READ jobs CONSTANT)
    Q_PROPERTY(QString lastAction READ lastAction NOTIFY lastActionChanged)

public:
    static CoreController* create(QQmlEngine* engine, QJSEngine* scriptEngine);
    static CoreController& instance();

    LibraryModel* library() { return &m_library; }
    SourcePluginModel* sources() { return &m_sources; }
    CatalogModel* catalog() { return &m_catalog; }
    JobModel* jobs() { return &m_jobs; }
    QString lastAction() const { return m_lastAction; }

    Q_INVOKABLE void launchGame(const QString& gameId);
    Q_INVOKABLE void searchCatalog(const QString& sourceId, const QString& query);
    Q_INVOKABLE void installCatalogEntry(const QString& entryId);
    Q_INVOKABLE void checkUpdates();

signals:
    void lastActionChanged();

private:
    explicit CoreController(QObject* parent = nullptr);

    void loadMockData();
    void setLastAction(const QString& action);
    QVector<CatalogEntry> mockCatalogFor(const QString& sourceId, const QString& query) const;

    LibraryModel m_library;
    SourcePluginModel m_sources;
    CatalogModel m_catalog;
    JobModel m_jobs;
    QString m_lastAction;
};

void registerCoreTypes();

} // namespace arachnel::core

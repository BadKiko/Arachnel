#pragma once

#include "source_plugin_model.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace arachnel::core {

class SettingsStore : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString libraryRoot READ libraryRoot WRITE setLibraryRoot NOTIFY libraryRootChanged)
    Q_PROPERTY(QString downloadsRoot READ downloadsRoot WRITE setDownloadsRoot NOTIFY downloadsRootChanged)

public:
    explicit SettingsStore(QObject* parent = nullptr);

    QString libraryRoot() const { return m_libraryRoot; }
    QString downloadsRoot() const { return m_downloadsRoot; }

    QVector<SourcePluginInfo> sources() const { return m_sources; }
    void setSources(QVector<SourcePluginInfo> sources);
    void persistSources(QVector<SourcePluginInfo> sources);
    QString catalogUrlForSource(const QString& sourceId) const;

    QString resolvedLibraryRoot() const;
    QString resolvedDownloadsRoot() const;

    void setLibraryRoot(const QString& path);
    void setDownloadsRoot(const QString& path);

    void load();
    void save();

signals:
    void libraryRootChanged();
    void downloadsRootChanged();
    void sourcesChanged();

private:
    QString m_libraryRoot;
    QString m_downloadsRoot;
    QVector<SourcePluginInfo> m_sources;
};

} // namespace arachnel::core

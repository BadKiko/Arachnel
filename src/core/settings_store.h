#pragma once

#include "source_plugin_model.h"
#include "storage_library.h"
#include "storage_library_model.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace arachnel::core {

class SettingsStore : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString libraryRoot READ libraryRoot WRITE setLibraryRoot NOTIFY libraryRootChanged)
    Q_PROPERTY(QString downloadsRoot READ downloadsRoot WRITE setDownloadsRoot NOTIFY downloadsRootChanged)
    Q_PROPERTY(StorageLibraryModel* storageLibraries READ storageLibraries CONSTANT)
    Q_PROPERTY(int maxConcurrentDownloads READ maxConcurrentDownloads WRITE setMaxConcurrentDownloads
                   NOTIFY maxConcurrentDownloadsChanged)
    Q_PROPERTY(bool autoCheckUpdates READ autoCheckUpdates WRITE setAutoCheckUpdates NOTIFY
                   autoCheckUpdatesChanged)
    Q_PROPERTY(bool verifyPortableFiles READ verifyPortableFiles WRITE setVerifyPortableFiles NOTIFY
                   verifyPortableFilesChanged)

public:
    explicit SettingsStore(QObject* parent = nullptr);

    QString libraryRoot() const { return m_libraryRoot; }
    QString downloadsRoot() const { return m_downloadsRoot; }
    int maxConcurrentDownloads() const { return m_maxConcurrentDownloads; }
    bool autoCheckUpdates() const { return m_autoCheckUpdates; }
    bool verifyPortableFiles() const { return m_verifyPortableFiles; }
    StorageLibraryModel* storageLibraries() { return &m_storageLibraries; }
    const StorageLibraryModel* storageLibraries() const { return &m_storageLibraries; }

    QVector<SourcePluginInfo> sources() const { return m_sources; }
    void setSources(QVector<SourcePluginInfo> sources);
    void persistSources(QVector<SourcePluginInfo> sources);
    QString catalogUrlForSource(const QString& sourceId) const;

    QHash<QString, bool> pluginEnabledStates() const { return m_pluginEnabledStates; }
    void setPluginEnabledStates(QHash<QString, bool> states);
    bool pluginEnabled(const QString& id, bool defaultValue = true) const;

    QString resolvedLibraryRoot() const;
    QString resolvedDownloadsRoot() const;
    QString resolvedLibraryRoot(const QString& libraryId) const;
    QString resolvedDownloadsRoot(const QString& libraryId) const;
    QString defaultLibraryId() const;
    QString gameDirFor(const QString& libraryId, const QString& gameId) const;

    void syncLegacyRootsFromLibrary(const StorageLibrary& library);

    void setLibraryRoot(const QString& path);
    void setDownloadsRoot(const QString& path);
    void setMaxConcurrentDownloads(int count);
    void setAutoCheckUpdates(bool enabled);
    void setVerifyPortableFiles(bool enabled);

    void load();
    void save();

signals:
    void libraryRootChanged();
    void downloadsRootChanged();
    void sourcesChanged();
    void maxConcurrentDownloadsChanged();
    void autoCheckUpdatesChanged();
    void verifyPortableFilesChanged();
    void pluginStatesChanged();

private:
    void ensureDefaultStorageLibraries();

    QString m_libraryRoot;
    QString m_downloadsRoot;
    int m_maxConcurrentDownloads = 2;
    bool m_autoCheckUpdates = true;
    bool m_verifyPortableFiles = true;
    QVector<SourcePluginInfo> m_sources;
    QHash<QString, bool> m_pluginEnabledStates;
    StorageLibraryModel m_storageLibraries;
};

} // namespace arachnel::core

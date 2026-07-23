#pragma once

#include "catalog_types.h"
#include "library_model.h"

#include <QStringList>
#include <functional>

namespace arachnel::core {

class CatalogModel;
class GameMetadataService;
class JobStore;
class LibraryStore;
class PluginHost;
class SettingsStore;
struct JobEntry;

class LibraryController
{
public:
    struct Hooks {
        std::function<void()> syncLibrary;
        std::function<void(const QString&)> removeJobs;
        std::function<void(const QString&)> notice;
        /** Delete game folders off the UI thread. Paths are unique; title for notices. */
        std::function<void(const QStringList& paths, const QString& title)> deleteGameFilesAsync;
        std::function<const CatalogEntry*(const QString&)> findCatalogEntry;
        std::function<const JobEntry*(const QString&)> findLatestJob;
        std::function<QString(const QString&)> sourceWebsiteFor;
        std::function<InstallKind(const QString&, const QString&)> detectInstallKind;
    };

    LibraryController(LibraryModel* library, CatalogModel* catalog, LibraryStore* store,
                      JobStore* jobs, SettingsStore* settings, PluginHost* plugins,
                      GameMetadataService* metadata, Hooks hooks);

    bool isEntryPlayable(const QString& entryId) const;
    bool isEntryDownloadComplete(const QString& entryId) const;
    bool entryDownloadFilesExist(const QString& entryId) const;
    QVariantMap entryDetails(const QString& entryId) const;
    void setGameAutoUpdate(const QString& entryId, bool enabled);
    void setGameLaunchArgs(const QString& entryId, const QString& args);
    void setGameExecutableOverride(const QString& entryId, const QString& path);
    void setGameProtonId(const QString& entryId, const QString& protonId);
    void setGameOnlineFixEnabled(const QString& entryId, bool enabled);
    void removeGame(const QString& gameId, bool deleteFiles);
    void removeEntry(const QString& entryId, bool deleteFiles);
    void moveGame(const QString& gameId, const QString& targetLibraryId);
    QVariantList gamesOnLibrary(const QString& libraryId) const;
    /** Remove a storage drive. If games remain and force is false, returns false.
     *  With force, reassigns games to another drive (no file moves) then removes the drive. */
    bool removeStorageLibrary(const QString& libraryId, bool force);
    /** Scan storage roots for on-disk installs missing from library.json. Returns newly added count. */
    int scanInstalledGames();

private:
    void sync() const;
    LibraryModel* m_library;
    CatalogModel* m_catalog;
    LibraryStore* m_store;
    JobStore* m_jobs;
    SettingsStore* m_settings;
    PluginHost* m_plugins;
    GameMetadataService* m_metadata;
    Hooks m_hooks;
};

} // namespace arachnel::core

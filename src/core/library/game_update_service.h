#pragma once

#include "catalog_types.h"
#include "library_model.h"

#include <functional>

namespace arachnel::core {

class JobOrchestrator;
class LibraryStore;
class PluginHost;
class SettingsStore;

class GameUpdateService
{
public:
    struct Hooks {
        std::function<void()> syncLibrary;
        std::function<void(const QString&)> notice;
        std::function<void()> refreshCatalog;
        std::function<bool(const QString&)> entryPlayable;
        std::function<bool(const QString&)> entryHasActiveJob;
    };

    GameUpdateService(LibraryStore* store, SettingsStore* settings, PluginHost* plugins,
                      JobOrchestrator* jobs, const QVector<CatalogEntry>* catalog, Hooks hooks);
    bool gameHasUpdate(const LibraryGame& game, const CatalogEntry& remote) const;
    int recalculateLibraryUpdates(bool notify);
    void checkUpdates(bool catalogAvailable);
    void runAutoInstallUpdates();

private:
    bool isRemoteUploadDateNewer(const QString& remote, const QString& local) const;
    LibraryStore* m_store;
    SettingsStore* m_settings;
    PluginHost* m_plugins;
    JobOrchestrator* m_jobs;
    const QVector<CatalogEntry>* m_catalog;
    Hooks m_hooks;
};

} // namespace arachnel::core

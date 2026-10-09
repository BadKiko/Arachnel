#pragma once

#include "plugin_interface.h"
#include "source_plugin_model.h"

#include <QObject>
#include <QFuture>
#include <QHash>
#include <QLibrary>
#include <QMutex>
#include <QPair>
#include <QString>
#include <QVector>
#include <cstddef>
#include <functional>

namespace arachnel::core {

struct LoadedPlugin {
    QString rootPath;
    SourcePluginInfo info;
    QLibrary library;
    ISourcePlugin* instance = nullptr;
    void (*destroyFn)(ISourcePlugin*) = nullptr;
    int (*catalogJsonFn)(ISourcePlugin*, char**, size_t*) = nullptr;
    void (*catalogJsonFreeFn)(char*) = nullptr;
    /** Optional catalog-source extension (null when the plugin does not export it). */
    int (*sourceExtVersionFn)() = nullptr;
    int (*catalogSourceFn)(ISourcePlugin*, char**, size_t*) = nullptr;
    int (*normalizeRowsFn)(ISourcePlugin*, const char*, size_t, char**, size_t*) = nullptr;
    void (*sourceFreeFn)(char*) = nullptr;
    int apiVersion = 0;
    /** True when CatalogEntry sizeof matched (safe for entryById / detectUpdate). */
    bool catalogEntryLayoutTrusted = false;
};

class PluginHost : public QObject
{
    Q_OBJECT

public:
    explicit PluginHost(QObject* parent = nullptr);
    ~PluginHost() override;

    void scan();
    void shutdownPlugins();
    QVector<SourcePluginInfo> pluginInfos() const;
    ISourcePlugin* plugin(const QString& id) const;
    /**
     * Load catalog entries for a plugin.
     * API 4+: JSON across the DLL boundary (no CatalogEntry layout).
     * API 2/3: calls plugin->catalog() directly.
     */
    QVector<CatalogEntry> loadPluginCatalog(const QString& id) const;
    /**
     * Raw catalog JSON; optional fingerprint out-param. With `persist` (default) the payload is
     * also written to the disk cache; callers that save a parsed snapshot first pass false and
     * persist afterwards, so the cache key and the snapshot never disagree.
     */
    QByteArray loadPluginCatalogPayload(const QString& id, QByteArray* payloadSha = nullptr,
                                        bool persist = true) const;
    bool hasPlugin(const QString& id) const;
    /**
     * True when the plugin exports the optional catalog-source extension at a version this host
     * understands (see plugin_api.h). Never true for plugins that only offer catalog_json.
     */
    bool pluginHasCatalogSource(const QString& id) const;
    /** JSON descriptor from the plugin (empty on failure). Safe to call from a worker. */
    QByteArray pluginCatalogSourceDescriptor(const QString& id) const;
    /** Normalizes one JSON array of raw rows; returns plugin JSON or empty on failure. */
    QByteArray pluginNormalizeRows(const QString& id, const char* rows, size_t len) const;
    /** True if plugin.json exists under a search root (even if DLL failed to load). */
    bool hasPluginFilesOnDisk(const QString& id) const;
    /** Version from on-disk plugin.json (empty if missing). */
    QString pluginVersionOnDisk(const QString& id) const;
    /** All plugin.json packages under search roots (loaded or not). */
    QVector<SourcePluginInfo> diskPluginInfos() const;
    QStringList pluginIds() const;
    int count() const { return m_plugins.size(); }

    static QString writablePluginsDir();
    static bool openWritablePluginsDir();
    QString lastError() const { return m_lastError; }

    bool installFromArach(const QString& archivePath);
    bool uninstallPlugin(const QString& pluginId);

    using InstallCallback = std::function<void(const InstallResult&)>;
    using OwnedProgressCallback = std::function<void(const OwnedDownloadProgress&)>;
    void runInstallAsync(ISourcePlugin* plugin, const InstallContext& ctx, InstallCallback callback);
    void runAddonInstallAsync(ISourcePlugin* plugin, const AddonInstallContext& ctx,
                              InstallCallback callback);
    void runOwnedDownloadAsync(ISourcePlugin* plugin, const InstallContext& ctx,
                               OwnedProgressCallback onProgress, InstallCallback onFinished);
    void cancelOwnedDownload(const QString& pluginId, const QString& jobId);
    void setOwnedDownloadPaused(const QString& pluginId, const QString& jobId, bool paused);

    /** Called immediately before unloading plugin DLLs (wait for catalog futures). */
    void setBeforeUnloadHook(std::function<void()> hook);
    /** Block until install / owned-download workers leave plugin code. */
    void waitForInFlightPluginWorkers();
    bool hasInFlightPluginWorkers() const;

    static QStringList pluginSearchRoots();
    /** Copy plugins from install-dir / legacy AppData into the writable plugins folder. */
    static void migratePluginTrees();

    bool pluginOwnsDownload(const QString& pluginId) const;
    int pluginApiVersion(const QString& pluginId) const;
    /**
     * True when CatalogEntry may cross the DLL (entryById / detectUpdate).
     * API 4 JSON catalog still works when this is false.
     */
    bool pluginCatalogEntryLayoutTrusted(const QString& pluginId) const;
    /** Human-readable reason the last loadPluginDir failed (empty if ok). */
    QString lastLoadRejectReason() const { return m_lastLoadRejectReason; }
    /**
     * On-disk plugins that did not load, with a short reason (minArachnel, ABI, …).
     * Used to tell the user to update Arachnel instead of silently skipping.
     */
    QVector<QPair<QString, QString>> incompatibleDiskPlugins() const;

signals:
    void pluginsChanged();

private:
    bool loadPluginDir(const QString& dirPath);
    void setLoadRejectReason(const QString& reason);
    /** Load one plugin id from disk search roots (no unloadAll). */
    bool loadPluginById(const QString& pluginId);
    void unloadAll();
    /** Drop one loaded plugin (DLL unlock for replace/uninstall). */
    void unloadPlugin(const QString& pluginId);
    static QString resolveLibraryFile(const QString& pluginDir, const QString& libraryBase);
    static bool extractArachArchive(const QString& archivePath, const QString& destDir,
                                    QString* errorOut);
    static bool findPluginBundleRoot(const QString& extractedDir, QString* bundleRootOut);

    QHash<QString, LoadedPlugin*> m_plugins;
    QString m_lastError;
    QString m_lastLoadRejectReason;
    std::function<void()> m_beforeUnload;
    int m_scanDepth = 0;
    mutable QMutex m_pluginWorkerMutex;
    QList<QFuture<void>> m_pluginWorkerFutures;

    void trackPluginWorker(QFuture<void> future);
};

} // namespace arachnel::core

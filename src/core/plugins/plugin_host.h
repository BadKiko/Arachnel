#pragma once

#include "plugin_interface.h"
#include "source_plugin_model.h"

#include <QObject>
#include <QHash>
#include <QLibrary>
#include <QString>
#include <QVector>
#include <functional>

namespace arachnel::core {

struct LoadedPlugin {
    QString rootPath;
    SourcePluginInfo info;
    QLibrary library;
    ISourcePlugin* instance = nullptr;
    void (*destroyFn)(ISourcePlugin*) = nullptr;
    int apiVersion = 0;
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
    bool hasPlugin(const QString& id) const;
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

    /** Called immediately before unloadAll during install/uninstall (wait for catalog futures). */
    void setBeforeUnloadHook(std::function<void()> hook);

    static QStringList pluginSearchRoots();
    /** Copy plugins from install-dir / legacy AppData into the writable plugins folder. */
    static void migratePluginTrees();

    bool pluginOwnsDownload(const QString& pluginId) const;
    int pluginApiVersion(const QString& pluginId) const;

signals:
    void pluginsChanged();

private:
    bool loadPluginDir(const QString& dirPath);
    void unloadAll();
    static QString resolveLibraryFile(const QString& pluginDir, const QString& libraryBase);
    static bool extractArachArchive(const QString& archivePath, const QString& destDir,
                                    QString* errorOut);
    static bool findPluginBundleRoot(const QString& extractedDir, QString* bundleRootOut);

    QHash<QString, LoadedPlugin*> m_plugins;
    QString m_lastError;
    std::function<void()> m_beforeUnload;
};

} // namespace arachnel::core

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
};

class PluginHost : public QObject
{
    Q_OBJECT

public:
    explicit PluginHost(QObject* parent = nullptr);
    ~PluginHost() override;

    void scan();
    QVector<SourcePluginInfo> pluginInfos() const;
    ISourcePlugin* plugin(const QString& id) const;
    bool hasPlugin(const QString& id) const;
    int count() const { return m_plugins.size(); }

    static QString writablePluginsDir();
    static bool openWritablePluginsDir();
    QString lastError() const { return m_lastError; }

    bool installFromArach(const QString& archivePath);

    using InstallCallback = std::function<void(const InstallResult&)>;
    void runInstallAsync(ISourcePlugin* plugin, const InstallContext& ctx, InstallCallback callback);
    void runAddonInstallAsync(ISourcePlugin* plugin, const AddonInstallContext& ctx,
                              InstallCallback callback);

    static QStringList pluginSearchRoots();

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
};

} // namespace arachnel::core

#include "plugin_host.h"

#include "crash_log.h"
#include "catalog_types.h"

#include "plugin_api.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace arachnel::core {

void PluginHost::runInstallAsync(ISourcePlugin* plugin, const InstallContext& ctx,
                                 InstallCallback callback)
{
    if (!plugin) {
        InstallResult result;
        result.success = false;
        result.error = QCoreApplication::translate("Core", "Plugin not found");
        callback(result);
        return;
    }

    (void)QtConcurrent::run([plugin, ctx, callback]() {
        const InstallResult result = plugin->installFromDownload(ctx);
        QObject* app = QCoreApplication::instance();
        if (!app) {
            callback(result);
            return;
        }
        QTimer::singleShot(0, app, [callback, result]() { callback(result); });
    });
}

void PluginHost::runAddonInstallAsync(ISourcePlugin* plugin, const AddonInstallContext& ctx,
                                      InstallCallback callback)
{
    if (!plugin) {
        InstallResult result;
        result.success = false;
        result.error = QCoreApplication::translate("Core", "Plugin not found");
        callback(result);
        return;
    }

    (void)QtConcurrent::run([plugin, ctx, callback]() {
        const InstallResult result = plugin->installAddonFromDownload(ctx);
        QObject* app = QCoreApplication::instance();
        if (!app) {
            callback(result);
            return;
        }
        QTimer::singleShot(0, app, [callback, result]() { callback(result); });
    });
}

void PluginHost::runOwnedDownloadAsync(ISourcePlugin* plugin, const InstallContext& ctx,
                                       OwnedProgressCallback onProgress, InstallCallback onFinished)
{
    if (!plugin) {
        InstallResult result;
        result.success = false;
        result.error = QCoreApplication::translate("Core", "Plugin not found");
        if (onFinished)
            onFinished(result);
        return;
    }

    (void)QtConcurrent::run([plugin, ctx, onProgress, onFinished]() {
        auto progressBridge = [onProgress](const OwnedDownloadProgress& p) {
            if (!onProgress)
                return;
            QObject* app = QCoreApplication::instance();
            if (!app) {
                onProgress(p);
                return;
            }
            QTimer::singleShot(0, app, [onProgress, p]() { onProgress(p); });
        };
        const InstallResult result = plugin->startOwnedDownload(ctx, progressBridge);
        QObject* app = QCoreApplication::instance();
        if (!app) {
            if (onFinished)
                onFinished(result);
            return;
        }
        QTimer::singleShot(0, app, [onFinished, result]() {
            if (onFinished)
                onFinished(result);
        });
    });
}

void PluginHost::cancelOwnedDownload(const QString& pluginId, const QString& jobId)
{
    ISourcePlugin* instance = plugin(pluginId);
    if (!instance)
        return;
    instance->cancelOwnedDownload(jobId);
}

bool PluginHost::pluginOwnsDownload(const QString& pluginId) const
{
    const auto it = m_plugins.constFind(pluginId);
    if (it == m_plugins.constEnd() || !it.value())
        return false;
    if (it.value()->apiVersion < 3)
        return false;
    return it.value()->info.capabilities.contains(QStringLiteral("owns_download"));
}

int PluginHost::pluginApiVersion(const QString& pluginId) const
{
    const auto it = m_plugins.constFind(pluginId);
    if (it == m_plugins.constEnd() || !it.value())
        return 0;
    return it.value()->apiVersion;
}

QString PluginHost::writablePluginsDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty())
        return {};
    const QString dir = base + QStringLiteral("/plugins");
    QDir().mkpath(dir);
    return dir;
}

bool PluginHost::openWritablePluginsDir()
{
    const QString dir = writablePluginsDir();
    if (dir.isEmpty())
        return false;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}


} // namespace arachnel::core

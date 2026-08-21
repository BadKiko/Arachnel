#include "plugin_host.h"

#include "crash_log.h"
#include "catalog_types.h"
#include "plugin_api.h"
#include "plugin_version.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
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

void PluginHost::trackPluginWorker(QFuture<void> future)
{
    QMutexLocker lock(&m_pluginWorkerMutex);
    // Drop finished slots so the list does not grow forever across many installs.
    QList<QFuture<void>> live;
    live.reserve(m_pluginWorkerFutures.size() + 1);
    for (QFuture<void>& existing : m_pluginWorkerFutures) {
        if (!existing.isFinished())
            live.append(std::move(existing));
    }
    live.append(std::move(future));
    m_pluginWorkerFutures.swap(live);
}

void PluginHost::waitForInFlightPluginWorkers()
{
    for (;;) {
        QList<QFuture<void>> futures;
        {
            QMutexLocker lock(&m_pluginWorkerMutex);
            futures.swap(m_pluginWorkerFutures);
        }
        if (futures.isEmpty())
            break;
        for (QFuture<void>& future : futures)
            future.waitForFinished();
    }
}

bool PluginHost::hasInFlightPluginWorkers() const
{
    QMutexLocker lock(&m_pluginWorkerMutex);
    for (const QFuture<void>& future : m_pluginWorkerFutures) {
        if (!future.isFinished())
            return true;
    }
    return false;
}

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

    QFuture<void> future = QtConcurrent::run([plugin, ctx, callback]() {
        const InstallResult result = plugin->installFromDownload(ctx);
        QObject* app = QCoreApplication::instance();
        if (!app) {
            callback(result);
            return;
        }
        QTimer::singleShot(0, app, [callback, result]() { callback(result); });
    });
    trackPluginWorker(std::move(future));
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

    QFuture<void> future = QtConcurrent::run([plugin, ctx, callback]() {
        const InstallResult result = plugin->installAddonFromDownload(ctx);
        QObject* app = QCoreApplication::instance();
        if (!app) {
            callback(result);
            return;
        }
        QTimer::singleShot(0, app, [callback, result]() { callback(result); });
    });
    trackPluginWorker(std::move(future));
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

    QFuture<void> future = QtConcurrent::run([plugin, ctx, onProgress, onFinished]() {
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
    trackPluginWorker(std::move(future));
}

void PluginHost::cancelOwnedDownload(const QString& pluginId, const QString& jobId)
{
    ISourcePlugin* instance = plugin(pluginId);
    if (!instance)
        return;
    instance->cancelOwnedDownload(jobId);
}

void PluginHost::setOwnedDownloadPaused(const QString& pluginId, const QString& jobId, bool paused)
{
    ISourcePlugin* instance = plugin(pluginId);
    if (!instance)
        return;
    instance->setOwnedDownloadPaused(jobId, paused);
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

bool PluginHost::pluginCatalogEntryLayoutTrusted(const QString& pluginId) const
{
    const auto it = m_plugins.constFind(pluginId);
    if (it == m_plugins.constEnd() || !it.value())
        return false;
    return it.value()->catalogEntryLayoutTrusted;
}

QVector<QPair<QString, QString>> PluginHost::incompatibleDiskPlugins() const
{
    QVector<QPair<QString, QString>> out;
    for (const SourcePluginInfo& disk : diskPluginInfos()) {
        if (hasPlugin(disk.id))
            continue;

        QFile file(disk.pluginRootPath + QStringLiteral("/plugin.json"));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
        const QString name = obj.value(QStringLiteral("name")).toString(disk.id);
        const int apiVersion = obj.value(QStringLiteral("apiVersion")).toInt(1);
        const QString minArachnel = obj.value(QStringLiteral("minArachnel")).toString();
        const QString maxArachnel = obj.value(QStringLiteral("maxArachnel")).toString();
        const QString appVersion = QCoreApplication::applicationVersion();

        QString reason;
        if (apiVersion < ARACHNEL_PLUGIN_API_VERSION_MIN
            || apiVersion > ARACHNEL_PLUGIN_API_VERSION) {
            reason = QCoreApplication::translate(
                         "Core",
                         "%1 needs a different Arachnel plugin API. Update the app or reinstall "
                         "a matching plugin.")
                         .arg(name);
        } else if (!appVersionInRange(appVersion, minArachnel, maxArachnel)) {
            const QString needMin =
                minArachnel.trimmed().isEmpty() ? QStringLiteral("0.0.0") : minArachnel.trimmed();
            if (!maxArachnel.trimmed().isEmpty()
                && compareAppVersions(appVersion, maxArachnel.trimmed()) > 0) {
                reason = QCoreApplication::translate(
                             "Core",
                             "%1 only supports Arachnel up to %2. Install a newer plugin build.")
                             .arg(name, maxArachnel.trimmed());
            } else {
                reason = QCoreApplication::translate(
                             "Core", "%1 needs Arachnel %2 or newer. Update the app.")
                             .arg(name, needMin);
            }
        } else {
            reason = QCoreApplication::translate(
                         "Core",
                         "%1 is installed but failed to load. Update Arachnel or reinstall the "
                         "plugin.")
                         .arg(name);
        }
        out.append({disk.id, reason});
    }
    return out;
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

#include "plugin_host.h"

#include "crash_log.h"
#include "catalog_types.h"
#include "file_utils.h"

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
#include <QSet>
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


#include "plugin_host_helpers.h"

PluginHost::PluginHost(QObject* parent)
    : QObject(parent)
{
#if defined(Q_OS_WIN)
    prependWindowsPathDirectory(QCoreApplication::applicationDirPath());
#endif
}

PluginHost::~PluginHost()
{
    unloadAll();
}

void PluginHost::unloadAll()
{
    if (m_beforeUnload)
        m_beforeUnload();

    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        LoadedPlugin* loaded = it.value();
        if (!loaded)
            continue;
        if (loaded->instance && loaded->destroyFn)
            loaded->destroyFn(loaded->instance);
        loaded->instance = nullptr;
        if (loaded->library.isLoaded())
            loaded->library.unload();
        delete loaded;
    }
    m_plugins.clear();
}

void PluginHost::setBeforeUnloadHook(std::function<void()> hook)
{
    m_beforeUnload = std::move(hook);
}

void PluginHost::shutdownPlugins()
{
    unloadAll();
}

QStringList PluginHost::pluginSearchRoots()
{
    QStringList roots;
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!dataDir.isEmpty())
        roots << dataDir + QStringLiteral("/plugins");

#if defined(Q_OS_WIN)
    const QByteArray roaming = qgetenv("APPDATA");
    if (!roaming.isEmpty()) {
        const QString legacy =
            QString::fromLocal8Bit(roaming) + QStringLiteral("/Arachnel/plugins");
        if (!roots.contains(legacy, Qt::CaseInsensitive))
            roots << legacy;
    }
#endif

    const QString sidecar =
        QCoreApplication::applicationDirPath() + QStringLiteral("/plugins");
    if (!roots.contains(sidecar, Qt::CaseInsensitive))
        roots << sidecar;

    return roots;
}

void PluginHost::migratePluginTrees()
{
    const QString destRoot = writablePluginsDir();
    if (destRoot.isEmpty())
        return;

    QStringList sources;
#if defined(Q_OS_WIN)
    const QByteArray roaming = qgetenv("APPDATA");
    if (!roaming.isEmpty())
        sources << QString::fromLocal8Bit(roaming) + QStringLiteral("/Arachnel/plugins");
#endif
    sources << QCoreApplication::applicationDirPath() + QStringLiteral("/plugins");

    for (const QString& sourceRoot : sources) {
        if (QDir::cleanPath(sourceRoot).compare(QDir::cleanPath(destRoot), Qt::CaseInsensitive) == 0)
            continue;
        QDir src(sourceRoot);
        if (!src.exists())
            continue;
        const QStringList ids = src.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString& id : ids) {
            const QString from = src.absoluteFilePath(id);
            const QString to = destRoot + QLatin1Char('/') + id;
            if (QDir(to).exists())
                continue;
            if (!QFileInfo::exists(from + QStringLiteral("/plugin.json")))
                continue;
            QString error;
            copyPathRecursive(from, to, &error);
        }
    }
}

void PluginHost::scan()
{
    migratePluginTrees();
    unloadAll();

    const QStringList roots = pluginSearchRoots();
    for (const QString& root : roots) {
        QDir rootDir(root);
        if (!rootDir.exists())
            continue;

        const QStringList entries =
            rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString& entry : entries) {
            if (entry.endsWith(QStringLiteral(".staging")) || entry.endsWith(QStringLiteral(".bak")))
                continue;
            const QString pluginDir = rootDir.absoluteFilePath(entry);
            loadPluginDir(pluginDir);
        }
    }

    emit pluginsChanged();
}

QString PluginHost::resolveLibraryFile(const QString& pluginDir, const QString& libraryBase)
{
    const QStringList candidates = {
        platformLibraryName(libraryBase),
#if defined(Q_OS_WIN)
        QStringLiteral("lib") + libraryBase + QStringLiteral(".dll"),
#endif
    };
    for (const QString& fileName : candidates) {
        const QString candidate = pluginDir + QLatin1Char('/') + fileName;
        if (QFile::exists(candidate))
            return candidate;
    }
    return {};
}

bool PluginHost::loadPluginDir(const QString& dirPath)
{
    const QString manifestPath = dirPath + QStringLiteral("/plugin.json");
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly))
        return false;

    const QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
    const QString id = manifest.value(QStringLiteral("id")).toString();
    const QString libraryBase = manifest.value(QStringLiteral("library")).toString();
    const int apiVersion = manifest.value(QStringLiteral("apiVersion")).toInt(1);

    if (id.isEmpty() || libraryBase.isEmpty())
        return false;
    if (apiVersion < ARACHNEL_PLUGIN_API_VERSION_MIN || apiVersion > ARACHNEL_PLUGIN_API_VERSION) {
        logDiagnostic(QStringLiteral("Plugin rejected (apiVersion %1, allowed %2..%3): %4")
                          .arg(apiVersion)
                          .arg(ARACHNEL_PLUGIN_API_VERSION_MIN)
                          .arg(ARACHNEL_PLUGIN_API_VERSION)
                          .arg(dirPath));
        return false;
    }
    if (m_plugins.contains(id))
        return false;

    const QString libraryPath = resolveLibraryFile(dirPath, libraryBase);
    if (libraryPath.isEmpty())
        return false;

    auto* loaded = new LoadedPlugin();
    loaded->rootPath = dirPath;
    loaded->library.setFileName(libraryPath);
#if defined(Q_OS_WIN)
    ScopedAddDllDirectory dllDirectories(
        {QCoreApplication::applicationDirPath(), QFileInfo(libraryPath).absolutePath()});
#else
    const QString appDir = QCoreApplication::applicationDirPath();
    const QByteArray ldKey("LD_LIBRARY_PATH");
    const QByteArray previousLd = qgetenv(ldKey);
    const QByteArray appDirUtf8 = appDir.toUtf8();
    if (!previousLd.contains(appDirUtf8)) {
        QByteArray merged = appDirUtf8;
        if (!previousLd.isEmpty()) {
            merged += ':';
            merged += previousLd;
        }
        qputenv(ldKey, merged);
    }
#endif
    if (!loaded->library.load()) {
        g_lastPluginLoadError = loaded->library.errorString();
        logDiagnostic(QStringLiteral("Plugin library load failed for %1: %2")
                          .arg(libraryPath, g_lastPluginLoadError));
        delete loaded;
        return false;
    }
    g_lastPluginLoadError.clear();

    auto resolvePluginFn = [&](const char* name) -> QFunctionPointer {
        QFunctionPointer symbol = loaded->library.resolve(name);
#if defined(Q_OS_WIN)
        if (!symbol) {
            const HMODULE module =
                LoadLibraryW(reinterpret_cast<LPCWSTR>(libraryPath.utf16()));
            if (module)
                symbol = reinterpret_cast<QFunctionPointer>(GetProcAddress(module, name));
        }
#endif
        return symbol;
    };

    auto* apiVersionFn = reinterpret_cast<int (*)()>(resolvePluginFn("arachnel_plugin_api_version"));
    auto* catalogEntrySizeFn =
        reinterpret_cast<int (*)()>(resolvePluginFn("arachnel_plugin_catalog_entry_size"));
    auto* createFn = reinterpret_cast<ISourcePlugin* (*)(const char*)>(
        resolvePluginFn("arachnel_plugin_create"));
    auto* destroyFn = reinterpret_cast<void (*)(ISourcePlugin*)>(
        resolvePluginFn("arachnel_plugin_destroy"));

    if (!apiVersionFn || !createFn || !destroyFn) {
        loaded->library.unload();
        delete loaded;
        return false;
    }
    const int exportedApi = apiVersionFn();
    if (exportedApi < ARACHNEL_PLUGIN_API_VERSION_MIN
        || exportedApi > ARACHNEL_PLUGIN_API_VERSION) {
        loaded->library.unload();
        delete loaded;
        return false;
    }
    if (catalogEntrySizeFn) {
        const int pluginEntrySize = catalogEntrySizeFn();
        const int coreEntrySize = static_cast<int>(sizeof(CatalogEntry));
        logDiagnostic(QStringLiteral("Plugin %1 CatalogEntry size: plugin=%2 core=%3")
                          .arg(id)
                          .arg(pluginEntrySize)
                          .arg(coreEntrySize));
        if (pluginEntrySize != coreEntrySize) {
            logDiagnostic(QStringLiteral(
                              "Plugin rejected (CatalogEntry size mismatch): %1 plugin=%2 core=%3 "
                              "from %4 — rebuild plugins with run.ps1")
                              .arg(id)
                              .arg(pluginEntrySize)
                              .arg(coreEntrySize)
                              .arg(libraryPath));
            loaded->library.unload();
            delete loaded;
            return false;
        }
    } else {
        logDiagnostic(QStringLiteral(
                          "Plugin %1: catalog_entry_size export not resolved from %2 (library loaded=%3)")
                          .arg(id, libraryPath)
                          .arg(loaded->library.isLoaded() ? QStringLiteral("yes")
                                                          : QStringLiteral("no")));
    }

    loaded->instance = createFn(dirPath.toUtf8().constData());
    loaded->destroyFn = destroyFn;
    if (!loaded->instance) {
        loaded->library.unload();
        delete loaded;
        return false;
    }

    SourcePluginInfo info;
    info.id = loaded->instance->id();
    info.name = loaded->instance->name();
    info.description = loaded->instance->description();
    info.catalogUrl = manifest.value(QStringLiteral("catalogUrl")).toString();
    info.iconName = manifest.value(QStringLiteral("iconName")).toString(QStringLiteral("storefront"));
    info.enabled = true;
    info.isPlugin = true;
    // Prefer plugin.json version (CI bumps this); fall back to DLL if missing.
    const QString manifestVersion = manifest.value(QStringLiteral("version")).toString().trimmed();
    info.pluginVersion = !manifestVersion.isEmpty() ? manifestVersion : loaded->instance->version();
    info.pluginRootPath = dirPath;
    info.capabilities = loaded->instance->capabilities();
    info.apiVersion = exportedApi;
    loaded->info = info;
    loaded->apiVersion = exportedApi;

    m_plugins.insert(id, loaded);
    logDiagnostic(QStringLiteral("Plugin loaded: %1 v%2 from %3 (CatalogEntry=%4 bytes)")
                      .arg(info.id, info.pluginVersion, libraryPath)
                      .arg(sizeof(CatalogEntry)));
    return true;
}

QVector<SourcePluginInfo> PluginHost::pluginInfos() const
{
    QVector<SourcePluginInfo> infos;
    infos.reserve(m_plugins.size());
    for (auto it = m_plugins.constBegin(); it != m_plugins.constEnd(); ++it) {
        if (it.value())
            infos.append(it.value()->info);
    }
    return infos;
}

ISourcePlugin* PluginHost::plugin(const QString& id) const
{
    const auto it = m_plugins.constFind(id);
    if (it == m_plugins.constEnd() || !it.value())
        return nullptr;
    return it.value()->instance;
}

bool PluginHost::hasPlugin(const QString& id) const
{
    return m_plugins.contains(id);
}

bool PluginHost::hasPluginFilesOnDisk(const QString& id) const
{
    const QString trimmed = id.trimmed();
    if (trimmed.isEmpty())
        return false;

    for (const QString& root : pluginSearchRoots()) {
        const QString manifest =
            QDir(root).absoluteFilePath(trimmed + QStringLiteral("/plugin.json"));
        if (QFileInfo::exists(manifest))
            return true;
    }
    return false;
}

QString PluginHost::pluginVersionOnDisk(const QString& id) const
{
    const QString trimmed = id.trimmed();
    if (trimmed.isEmpty())
        return {};

    for (const QString& root : pluginSearchRoots()) {
        const QString manifest =
            QDir(root).absoluteFilePath(trimmed + QStringLiteral("/plugin.json"));
        QFile file(manifest);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
        const QString version = obj.value(QStringLiteral("version")).toString().trimmed();
        if (!version.isEmpty())
            return version;
    }
    return {};
}

QVector<SourcePluginInfo> PluginHost::diskPluginInfos() const
{
    QVector<SourcePluginInfo> infos;
    QSet<QString> seen;

    for (const QString& root : pluginSearchRoots()) {
        QDir rootDir(root);
        if (!rootDir.exists())
            continue;
        const QStringList entries =
            rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString& entry : entries) {
            if (entry.endsWith(QStringLiteral(".staging")) || entry.endsWith(QStringLiteral(".bak")))
                continue;
            const QString pluginDir = rootDir.absoluteFilePath(entry);
            QFile file(pluginDir + QStringLiteral("/plugin.json"));
            if (!file.open(QIODevice::ReadOnly))
                continue;
            const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
            const QString id = obj.value(QStringLiteral("id")).toString().trimmed();
            if (id.isEmpty() || seen.contains(id))
                continue;
            seen.insert(id);

            SourcePluginInfo info;
            info.id = id;
            info.name = obj.value(QStringLiteral("name")).toString(id);
            info.description = obj.value(QStringLiteral("description")).toString();
            info.pluginVersion = obj.value(QStringLiteral("version")).toString();
            info.pluginRootPath = pluginDir;
            info.iconName = obj.value(QStringLiteral("iconName")).toString(QStringLiteral("extension"));
            info.isPlugin = true;
            info.enabled = false;
            infos.append(info);
        }
    }
    return infos;
}

QStringList PluginHost::pluginIds() const
{
    return m_plugins.keys();
}

} // namespace arachnel::core

#include "plugin_host.h"

#include "../crash_log.h"
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

namespace {

QString platformLibraryName(const QString& base)
{
#if defined(Q_OS_WIN)
    return base + QStringLiteral(".dll");
#else
    return QStringLiteral("lib") + base + QStringLiteral(".so");
#endif
}

bool isZipArchive(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    char magic[4] = {};
    return file.read(magic, 4) == 4 && magic[0] == 'P' && magic[1] == 'K'
           && magic[2] == '\x03' && magic[3] == '\x04';
}

QString escapePowerShellSingleQuotedLiteral(const QString& value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    return escaped;
}

QString g_lastPluginLoadError;

#if defined(Q_OS_WIN)
void prependWindowsPathDirectory(const QString& directory)
{
    if (directory.isEmpty())
        return;

    const QByteArray pathKey = "PATH";
    const QByteArray entry = QDir::toNativeSeparators(directory).toUtf8();
    const QByteArray current = qgetenv(pathKey);
    if (current.split(';').contains(entry))
        return;

    qputenv(pathKey, entry + ";" + current);
}

struct ScopedAddDllDirectory {
    using AddDllDirectoryFn = DLL_DIRECTORY_COOKIE(WINAPI*)(PCWSTR);
    using RemoveDllDirectoryFn = BOOL(WINAPI*)(DLL_DIRECTORY_COOKIE);

    explicit ScopedAddDllDirectory(const QStringList& directories)
    {
        HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
        if (!kernel)
            return;

        const auto addDllDirectory = reinterpret_cast<AddDllDirectoryFn>(
            GetProcAddress(kernel, "AddDllDirectory"));
        removeDllDirectory = reinterpret_cast<RemoveDllDirectoryFn>(
            GetProcAddress(kernel, "RemoveDllDirectory"));
        if (!addDllDirectory || !removeDllDirectory)
            return;

        for (const QString& directory : directories) {
            if (directory.isEmpty())
                continue;
            const DLL_DIRECTORY_COOKIE cookie =
                addDllDirectory(reinterpret_cast<LPCWSTR>(directory.utf16()));
            if (cookie)
                cookies.append(cookie);
        }
    }

    ~ScopedAddDllDirectory()
    {
        if (!removeDllDirectory)
            return;
        for (const DLL_DIRECTORY_COOKIE cookie : cookies)
            removeDllDirectory(cookie);
    }

    RemoveDllDirectoryFn removeDllDirectory = nullptr;
    QList<DLL_DIRECTORY_COOKIE> cookies;
};
#endif

} // namespace

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

QStringList PluginHost::pluginSearchRoots()
{
    QStringList roots;
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!dataDir.isEmpty())
        roots << dataDir + QStringLiteral("/plugins");

    return roots;
}

void PluginHost::unloadAll()
{
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

void PluginHost::shutdownPlugins()
{
    unloadAll();
}

void PluginHost::scan()
{
    unloadAll();

    const QStringList roots = pluginSearchRoots();
    for (const QString& root : roots) {
        QDir rootDir(root);
        if (!rootDir.exists())
            continue;

        const QStringList entries =
            rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString& entry : entries) {
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

QStringList PluginHost::pluginIds() const
{
    return m_plugins.keys();
}

void PluginHost::runInstallAsync(ISourcePlugin* plugin, const InstallContext& ctx,
                                 InstallCallback callback)
{
    if (!plugin) {
        InstallResult result;
        result.success = false;
        result.error = QStringLiteral("Плагин не найден");
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
        result.error = QStringLiteral("Плагин не найден");
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
        result.error = QStringLiteral("Плагин не найден");
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

bool PluginHost::extractArachArchive(const QString& archivePath, const QString& destDir,
                                     QString* errorOut)
{
    QDir().mkpath(destDir);

    if (!isZipArchive(archivePath)) {
        if (errorOut) {
            *errorOut = QCoreApplication::translate(
                "Core", "Invalid plugin file. Choose a plugin package (.arach)");
        }
        return false;
    }

    QProcess process;
#if defined(Q_OS_WIN)
    // Prefer ZipFile: Expand-Archive rejects non-.zip extensions even when content is ZIP.
    process.setProgram(QStringLiteral("powershell"));
    const QString escapedArchive = escapePowerShellSingleQuotedLiteral(archivePath);
    const QString escapedDest = escapePowerShellSingleQuotedLiteral(destDir);
    process.setArguments({
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-Command"),
        QStringLiteral(
            "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
            "[System.IO.Compression.ZipFile]::ExtractToDirectory('%1', '%2')")
            .arg(escapedArchive, escapedDest),
    });
#else
    process.setProgram(QStringLiteral("unzip"));
    process.setArguments({QStringLiteral("-q"), archivePath, QStringLiteral("-d"), destDir});
#endif

    process.start();
    if (!process.waitForStarted(15000)) {
        if (errorOut) {
            *errorOut = QCoreApplication::translate("Core", "Could not start archive extraction");
        }
        return false;
    }
    if (!process.waitForFinished(300000)) {
        process.kill();
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Archive extraction timed out");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorOut) {
            const QByteArray stderrBytes = process.readAllStandardError();
            QString stderrText = QString::fromUtf8(stderrBytes).trimmed();
            if (stderrText.isEmpty() || stderrText.contains(QChar(0xFFFD)))
                stderrText = QString::fromLocal8Bit(stderrBytes).trimmed();
            *errorOut = stderrText.isEmpty()
                            ? QCoreApplication::translate("Core",
                                                          "Archive extraction failed (code %1)")
                                  .arg(process.exitCode())
                            : stderrText;
        }
        return false;
    }
    return true;
}

bool PluginHost::findPluginBundleRoot(const QString& extractedDir, QString* bundleRootOut)
{
    const QString directManifest = extractedDir + QStringLiteral("/plugin.json");
    if (QFile::exists(directManifest)) {
        *bundleRootOut = extractedDir;
        return true;
    }

    QDir dir(extractedDir);
    const QStringList children = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (children.size() == 1) {
        const QString nested = dir.absoluteFilePath(children.constFirst());
        if (QFile::exists(nested + QStringLiteral("/plugin.json"))) {
            *bundleRootOut = nested;
            return true;
        }
    }

    QDirIterator it(extractedDir, {QStringLiteral("plugin.json")}, QDir::Files,
                    QDirIterator::Subdirectories);
    if (it.hasNext()) {
        *bundleRootOut = QFileInfo(it.next()).absolutePath();
        return true;
    }
    return false;
}

bool PluginHost::installFromArach(const QString& archivePath)
{
    m_lastError.clear();

    QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.exists() || !archiveInfo.isFile()) {
        m_lastError = QStringLiteral("Файл не найден: %1").arg(archivePath);
        return false;
    }
    if (archiveInfo.suffix().compare(QStringLiteral("arach"), Qt::CaseInsensitive) != 0) {
        m_lastError = QStringLiteral("Поддерживаются только пакеты с расширением .arach");
        return false;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        m_lastError = QStringLiteral("Не удалось создать временную папку");
        return false;
    }

    QString extractError;
    if (!extractArachArchive(archiveInfo.absoluteFilePath(), tempDir.path(), &extractError)) {
        m_lastError = extractError;
        return false;
    }

    QString bundleRoot;
    if (!findPluginBundleRoot(tempDir.path(), &bundleRoot)) {
        m_lastError = QStringLiteral("В архиве нет plugin.json");
        return false;
    }

    QFile manifestFile(bundleRoot + QStringLiteral("/plugin.json"));
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("Не удалось прочитать plugin.json");
        return false;
    }

    const QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
    const QString id = manifest.value(QStringLiteral("id")).toString();
    const QString libraryBase = manifest.value(QStringLiteral("library")).toString();
    if (id.isEmpty() || libraryBase.isEmpty()) {
        m_lastError = QStringLiteral("Некорректный plugin.json");
        return false;
    }

    if (resolveLibraryFile(bundleRoot, libraryBase).isEmpty()) {
        m_lastError = QStringLiteral("В пакете нет библиотеки %1").arg(
            platformLibraryName(libraryBase));
        return false;
    }

    const QString targetRoot = writablePluginsDir() + QLatin1Char('/') + id;
    QDir targetDir(targetRoot);
    if (targetDir.exists()) {
        if (!targetDir.removeRecursively()) {
            m_lastError = QStringLiteral("Не удалось заменить существующий плагин");
            return false;
        }
    }

    if (!QDir().mkpath(targetRoot)) {
        m_lastError = QStringLiteral("Не удалось создать папку плагина");
        return false;
    }

    QDir bundleDir(bundleRoot);
    const QStringList files = bundleDir.entryList(QDir::Files);
    for (const QString& fileName : files) {
        if (!QFile::copy(bundleDir.absoluteFilePath(fileName),
                         targetRoot + QLatin1Char('/') + fileName)) {
            m_lastError = QStringLiteral("Не удалось скопировать %1").arg(fileName);
            return false;
        }
    }

    const QStringList subdirs = bundleDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& subdir : subdirs) {
        const QString srcSubdir = bundleDir.absoluteFilePath(subdir);
        const QString dstSubdir = targetRoot + QLatin1Char('/') + subdir;
        QDirIterator it(srcSubdir, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QString relativePath = QDir(srcSubdir).relativeFilePath(it.filePath());
            const QString destination = dstSubdir + QLatin1Char('/') + relativePath;
            if (!QDir().mkpath(QFileInfo(destination).path())) {
                m_lastError = QStringLiteral("Не удалось создать папку плагина");
                return false;
            }
            if (QFile::exists(destination) && !QFile::remove(destination)) {
                m_lastError = QStringLiteral("Не удалось заменить %1").arg(relativePath);
                return false;
            }
            if (!QFile::copy(it.filePath(), destination)) {
                m_lastError = QStringLiteral("Не удалось скопировать %1").arg(relativePath);
                return false;
            }
        }
    }

    scan();
    if (!hasPlugin(id)) {
        m_lastError = QCoreApplication::translate(
            "Core",
            "Plugin files were copied but the library failed to load. Rebuild the plugin for "
            "your Arachnel version and platform (MSVC/MinGW), then reinstall.");
        if (!g_lastPluginLoadError.isEmpty())
            m_lastError += QStringLiteral(" (") + g_lastPluginLoadError + QLatin1Char(')');
        return false;
    }

    return true;
}

bool PluginHost::uninstallPlugin(const QString& pluginId)
{
    m_lastError.clear();

    const QString id = pluginId.trimmed();
    if (id.isEmpty() || id.contains(QLatin1Char('/')) || id.contains(QLatin1Char('\\'))
        || id == QLatin1String(".") || id == QLatin1String("..")) {
        m_lastError = QCoreApplication::translate("Core", "Invalid plugin id");
        return false;
    }

    const QString targetRoot = writablePluginsDir() + QLatin1Char('/') + id;
    QDir targetDir(targetRoot);
    if (!targetDir.exists()) {
        m_lastError = QCoreApplication::translate("Core", "Plugin is not installed");
        return false;
    }

    // Drop loaded libraries before deleting files (DLL/.so stay locked otherwise).
    unloadAll();

    if (!targetDir.removeRecursively()) {
        m_lastError = QCoreApplication::translate("Core", "Could not delete plugin files");
        scan();
        return false;
    }

    scan();
    return true;
}

} // namespace arachnel::core

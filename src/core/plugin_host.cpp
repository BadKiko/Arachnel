#include "plugin_host.h"

#include "plugin_api.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QtConcurrent>

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

} // namespace

PluginHost::PluginHost(QObject* parent)
    : QObject(parent)
{
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
    if (apiVersion != ARACHNEL_PLUGIN_API_VERSION)
        return false;
    if (m_plugins.contains(id))
        return false;

    const QString libraryPath = resolveLibraryFile(dirPath, libraryBase);
    if (libraryPath.isEmpty())
        return false;

    auto* loaded = new LoadedPlugin();
    loaded->rootPath = dirPath;
    loaded->library.setFileName(libraryPath);
    if (!loaded->library.load()) {
        delete loaded;
        return false;
    }

    auto* apiVersionFn =
        reinterpret_cast<int (*)()>(loaded->library.resolve("arachnel_plugin_api_version"));
    auto* createFn = reinterpret_cast<ISourcePlugin* (*)(const char*)>(
        loaded->library.resolve("arachnel_plugin_create"));
    auto* destroyFn = reinterpret_cast<void (*)(ISourcePlugin*)>(
        loaded->library.resolve("arachnel_plugin_destroy"));

    if (!apiVersionFn || !createFn || !destroyFn) {
        loaded->library.unload();
        delete loaded;
        return false;
    }
    if (apiVersionFn() != ARACHNEL_PLUGIN_API_VERSION) {
        loaded->library.unload();
        delete loaded;
        return false;
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
    info.pluginVersion = loaded->instance->version();
    info.pluginRootPath = dirPath;
    info.capabilities = loaded->instance->capabilities();
    loaded->info = info;

    m_plugins.insert(id, loaded);
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
        QMetaObject::invokeMethod(app, [callback, result]() { callback(result); },
                                  Qt::QueuedConnection);
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
        QMetaObject::invokeMethod(app, [callback, result]() { callback(result); },
                                  Qt::QueuedConnection);
    });
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

    QString archiveForExtract = archivePath;
    QTemporaryDir zipCopyDir;
#if defined(Q_OS_WIN)
    // Expand-Archive only accepts .zip; .arach is ZIP under another extension.
    if (!zipCopyDir.isValid()) {
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось создать временную папку");
        return false;
    }
    archiveForExtract = zipCopyDir.path() + QStringLiteral("/package.zip");
    if (QFile::exists(archiveForExtract) && !QFile::remove(archiveForExtract)) {
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось подготовить архив к распаковке");
        return false;
    }
    if (!QFile::copy(archivePath, archiveForExtract)) {
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось прочитать пакет .arach");
        return false;
    }
#endif

    QProcess process;
#if defined(Q_OS_WIN)
    process.setProgram(QStringLiteral("powershell"));
    process.setArguments({QStringLiteral("-NoProfile"), QStringLiteral("-Command"),
                          QStringLiteral("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                              .arg(archiveForExtract, destDir)});
#else
    process.setProgram(QStringLiteral("unzip"));
    process.setArguments({QStringLiteral("-o"), archivePath, QStringLiteral("-d"), destDir});
#endif

    process.start();
    if (!process.waitForStarted(15000)) {
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось запустить распаковку архива");
        return false;
    }
    if (!process.waitForFinished(300000)) {
        process.kill();
        if (errorOut)
            *errorOut = QStringLiteral("Таймаут распаковки");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorOut) {
            const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *errorOut = stderrText.isEmpty()
                            ? QStringLiteral("Ошибка распаковки (код %1)").arg(process.exitCode())
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

    scan();
    if (!hasPlugin(id)) {
        m_lastError = QStringLiteral("Плагин скопирован, но не загрузился — проверьте совместимость");
        return false;
    }

    return true;
}

} // namespace arachnel::core

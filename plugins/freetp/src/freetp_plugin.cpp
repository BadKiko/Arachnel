#include "freetp_plugin.h"

#include "archive_installer.h"
#include "installer_runner.h"
#include "linux_fix_launch.h"

#include "catalog_parser.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace freetp {

namespace {

constexpr auto kSourceId = "freetp";

QByteArray readCatalogBytes(const QString& rootPath)
{
    const QString localPath = rootPath + QStringLiteral("/games-arachnel.json");
    QFile localFile(localPath);
    if (!localFile.open(QIODevice::ReadOnly))
        return {};
    return localFile.readAll();
}

bool shouldUseInnoInstaller(const QString& contentRoot,
                            const arachnel::core::InstallContext& ctx)
{
    if (ctx.installKind == arachnel::core::InstallKind::Installer)
        return true;

    const QString setupExe = findSetupExecutable(contentRoot);
    return !setupExe.isEmpty() && isInnoSetupExecutable(setupExe);
}

} // namespace

arachnel::core::WindowsRunEnv windowsRunEnvFromInstallContext(
    const arachnel::core::InstallContext& ctx)
{
    arachnel::core::WindowsRunEnv env;
    env.protonExecutable = ctx.protonExecutable;
    env.compatDataPath = ctx.compatDataPath;
    env.steamCompatClientPath = ctx.steamCompatClientPath;
    return env;
}

arachnel::core::WindowsRunEnv windowsRunEnvFromAddonContext(
    const arachnel::core::AddonInstallContext& ctx)
{
    arachnel::core::WindowsRunEnv env;
    env.protonExecutable = ctx.protonExecutable;
    env.compatDataPath = ctx.compatDataPath;
    env.steamCompatClientPath = ctx.steamCompatClientPath;
    return env;
}

FreetpPlugin::FreetpPlugin(QString rootPath)
    : m_rootPath(std::move(rootPath))
{
}

QString FreetpPlugin::id() const
{
    return QString::fromLatin1(kSourceId);
}

QString FreetpPlugin::name() const
{
    return QStringLiteral("FreeTP");
}

QString FreetpPlugin::description() const
{
    return QStringLiteral("Торрент-каталог FreeTP — portable и Inno Setup");
}

QString FreetpPlugin::version() const
{
    return QStringLiteral("1.0.0");
}

QStringList FreetpPlugin::capabilities() const
{
    return {QStringLiteral("search"), QStringLiteral("install"), QStringLiteral("update"),
            QStringLiteral("launch")};
}

void FreetpPlugin::resetCatalogCache()
{
    m_catalogLoaded = false;
    m_catalog.clear();
}

void FreetpPlugin::ensureCatalogLoaded() const
{
    if (m_catalogLoaded)
        return;
    m_catalogLoaded = true;

    const QByteArray payload = readCatalogBytes(m_rootPath);
    if (payload.isEmpty())
        return;

    m_catalog = arachnel::core::parseCatalogFeed(payload, id());
}

QVector<arachnel::core::CatalogEntry> FreetpPlugin::catalog() const
{
    ensureCatalogLoaded();
    return m_catalog;
}

QVector<arachnel::core::CatalogEntry> FreetpPlugin::search(const QString& query) const
{
    ensureCatalogLoaded();
    const QString needle = query.trimmed().toLower();
    if (needle.isEmpty())
        return m_catalog;

    QVector<arachnel::core::CatalogEntry> filtered;
    filtered.reserve(m_catalog.size());
    for (const auto& entry : m_catalog) {
        if (entry.title.toLower().contains(needle))
            filtered.append(entry);
    }
    return filtered;
}

std::optional<arachnel::core::CatalogEntry> FreetpPlugin::entryById(
    const QString& entryId) const
{
    ensureCatalogLoaded();
    for (const auto& entry : m_catalog) {
        if (entry.id == entryId)
            return entry;
    }
    return std::nullopt;
}

arachnel::core::InstallKind FreetpPlugin::detectInstallKindFromFileNames(
    const QStringList& fileNames) const
{
    bool hasSetup = false;
    bool hasFtpChunk = false;
    for (const QString& path : fileNames) {
        const QString fileName = QFileInfo(path).fileName().toLower();
        if (fileName == QStringLiteral("setup.exe"))
            hasSetup = true;
        if (fileName.endsWith(QStringLiteral(".ftp")))
            hasFtpChunk = true;
    }

    if (hasSetup || hasFtpChunk)
        return arachnel::core::InstallKind::Installer;
    return arachnel::core::InstallKind::PortableArchive;
}

arachnel::core::InstallKind FreetpPlugin::detectInstallKind(const QString& downloadPath) const
{
    const QString contentRoot = findDownloadContentRoot(downloadPath);
    if (contentRoot.isEmpty())
        return arachnel::core::InstallKind::PortableArchive;

    arachnel::core::InstallContext ctx;
    if (shouldUseInnoInstaller(contentRoot, ctx))
        return arachnel::core::InstallKind::Installer;
    return arachnel::core::InstallKind::PortableArchive;
}

arachnel::core::InstallResult FreetpPlugin::installFromDownload(
    const arachnel::core::InstallContext& ctx) const
{
    arachnel::core::InstallResult result;

    const QString contentRoot = findDownloadContentRoot(ctx.downloadPath);
    if (contentRoot.isEmpty() || !QDir(contentRoot).exists()) {
        result.success = false;
        result.error = QStringLiteral("Файлы загрузки не найдены");
        return result;
    }

    QString error;
    QString installPath;
    const arachnel::core::WindowsRunEnv runEnv = windowsRunEnvFromInstallContext(ctx);

    if (shouldUseInnoInstaller(contentRoot, ctx)) {
        const QString setupExe = findSetupExecutable(contentRoot);
        if (setupExe.isEmpty()) {
            result.success = false;
            result.error = QStringLiteral("Inno Setup не найден в загрузке");
            return result;
        }

        installPath = installInnoSetup(setupExe, ctx.targetPath, &error, runEnv);
        if (installPath.isEmpty()) {
            result.success = false;
            result.error = error.isEmpty() ? QStringLiteral("Ошибка тихой установки Inno Setup")
                                           : error;
            return result;
        }

        cleanupInnoSideEffects(installPath);
    } else {
        installPath = installPortableFromDownload(contentRoot, ctx.targetPath, &error);
        if (installPath.isEmpty()) {
            result.success = false;
            result.error = error.isEmpty() ? QStringLiteral("Ошибка portable-установки") : error;
            return result;
        }
    }

    result.success = true;
    result.installPath = installPath;
#if defined(Q_OS_LINUX)
    prepareLinuxFixInstall(installPath, m_rootPath);
#endif
    return result;
}

arachnel::core::InstallResult FreetpPlugin::installAddonFromDownload(
    const arachnel::core::AddonInstallContext& ctx) const
{
    arachnel::core::InstallResult result;
    if (ctx.gameInstallPath.isEmpty() || !QDir(ctx.gameInstallPath).exists()) {
        result.error = QStringLiteral("Сначала установите игру");
        return result;
    }
    if (ctx.downloadPath.isEmpty() || !QFileInfo::exists(ctx.downloadPath)) {
        result.error = QStringLiteral("Файлы дополнения не найдены");
        return result;
    }

    QString error;
    const arachnel::core::WindowsRunEnv runEnv = windowsRunEnvFromAddonContext(ctx);
    const QFileInfo artifact(ctx.downloadPath);
    if (artifact.isFile()) {
        const QString suffix = artifact.suffix().toLower();
        if (suffix == QStringLiteral("exe")) {
            if (installInnoOverlay(ctx.downloadPath, ctx.gameInstallPath, &error, runEnv).isEmpty()) {
                result.error = error.isEmpty() ? QStringLiteral("Ошибка установки фикса") : error;
                return result;
            }
            cleanupInnoSideEffects(ctx.gameInstallPath);
            result.success = true;
            result.installPath = ctx.gameInstallPath;
#if defined(Q_OS_LINUX)
            prepareLinuxFixInstall(ctx.gameInstallPath, m_rootPath);
#endif
            return result;
        }
    }

    const QString contentRoot = artifact.isDir() ? findDownloadContentRoot(ctx.downloadPath)
                                                 : artifact.absolutePath();
    const QString setupExe = findSetupExecutable(contentRoot);
    if (!setupExe.isEmpty() && isInnoSetupExecutable(setupExe)) {
        if (installInnoOverlay(setupExe, ctx.gameInstallPath, &error, runEnv).isEmpty()) {
            result.error = error.isEmpty() ? QStringLiteral("Ошибка установки фикса") : error;
            return result;
        }
        cleanupInnoSideEffects(ctx.gameInstallPath);
        result.success = true;
        result.installPath = ctx.gameInstallPath;
#if defined(Q_OS_LINUX)
        prepareLinuxFixInstall(ctx.gameInstallPath, m_rootPath);
#endif
        return result;
    }

    if (!installAddonOverlay(ctx.downloadPath, ctx.gameInstallPath, &error)) {
        result.error = error.isEmpty() ? QStringLiteral("Ошибка установки дополнения") : error;
        return result;
    }

    result.success = true;
    result.installPath = ctx.gameInstallPath;
#if defined(Q_OS_LINUX)
    prepareLinuxFixInstall(ctx.gameInstallPath, m_rootPath);
#endif
    return result;
}

std::optional<QString> FreetpPlugin::detectUpdate(const arachnel::core::LibraryGame& local,
                                                  const arachnel::core::CatalogEntry& remote) const
{
    if (remote.uploadDate.isEmpty() || local.uploadDate.isEmpty())
        return std::nullopt;

    const QDateTime remoteDate = QDateTime::fromString(remote.uploadDate, Qt::ISODate);
    const QDateTime localDate = QDateTime::fromString(local.uploadDate, Qt::ISODate);
    if (remoteDate.isValid() && localDate.isValid()) {
        if (remoteDate > localDate)
            return remote.uploadDate;
        return std::nullopt;
    }

    if (remote.uploadDate > local.uploadDate)
        return remote.uploadDate;
    return std::nullopt;
}

arachnel::core::LaunchInfo FreetpPlugin::launchInfo(const arachnel::core::LibraryGame& local) const
{
    arachnel::core::LaunchInfo info;
    if (local.installPath.isEmpty())
        return info;

    const QString exe = linuxFixLaunchEnabled() ? findBestGameExecutable(local.installPath)
                                                : findGameExecutable(local.installPath);
    if (exe.isEmpty())
        return info;

    info.executable = exe;
    info.workingDirectory = QFileInfo(exe).absolutePath();

#if defined(Q_OS_LINUX)
    LinuxFixLaunchOptions options;
    applyLinuxFixLaunchInfo(local.installPath, m_rootPath, options, &info);
#endif

    return info;
}

} // namespace freetp

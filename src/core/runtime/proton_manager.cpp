#include "proton_manager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>
#include <QEventLoop>

namespace arachnel::core {

namespace {

// GE-Proton archives are now arch-suffixed (e.g. GE-Proton11-5-x86_64 vs
// GE-Proton11-5-aarch64). A directory built for a different CPU silently
// cannot boot any Windows game, so filter it out of the available list.
bool isForeignArchGeProton(const QString& dirName)
{
    if (!dirName.startsWith(QStringLiteral("GE-Proton"), Qt::CaseInsensitive))
        return false;
    const bool isArm = dirName.contains(QStringLiteral("aarch64"), Qt::CaseInsensitive);
    const bool isX86 = dirName.contains(QStringLiteral("x86_64"), Qt::CaseInsensitive);
    if (!isArm && !isX86)
        return false;

    const QString arch = QSysInfo::currentCpuArchitecture();
    const bool hostArm = (arch == QLatin1String("arm64") || arch == QLatin1String("aarch64"));
    return hostArm ? isX86 : isArm;
}

} // namespace


#include "proton_manager_helpers.h"

ProtonManager::ProtonManager(QObject* parent)
    : QObject(parent)
{
}

QString ProtonManager::protonInstallRoot() const
{
    const QString root = appDataDir() + QStringLiteral("/proton");
    QDir().mkpath(root);
    return root;
}

QString ProtonManager::compatDataRoot() const
{
    const QString root = appDataDir() + QStringLiteral("/compatdata");
    QDir().mkpath(root);
    return root;
}

QString ProtonManager::findProtonScriptInDir(const QString& dir) const
{
    const QString direct = QDir(dir).filePath(QStringLiteral("proton"));
    if (QFileInfo::exists(direct))
        return direct;

    QDirIterator it(dir, QStringList{QStringLiteral("proton")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (QFileInfo(path).isExecutable())
            return path;
    }
    return {};
}

QString ProtonManager::makeEntryId(const QString& source, const QString& installDir) const
{
    const QByteArray digest =
        QCryptographicHash::hash(installDir.toUtf8(), QCryptographicHash::Sha1).toHex().left(16);
    return source + QLatin1Char(':') + QString::fromLatin1(digest);
}

void ProtonManager::appendEntry(QVector<ProtonEntry>* out, const QString& source,
                                const QString& sourceLabel, const QString& installDir,
                                const QString& displayName) const
{
    if (!out)
        return;

    const QString normalizedDir = normalizePath(installDir);
    if (normalizedDir.isEmpty() || findProtonScriptInDir(normalizedDir).isEmpty())
        return;

    for (const ProtonEntry& existing : *out) {
        if (existing.installDir == normalizedDir)
            return;
    }

    ProtonEntry entry;
    entry.id = makeEntryId(source, normalizedDir);
    entry.name = displayName;
    entry.installDir = normalizedDir;
    entry.source = source;
    entry.sourceLabel = sourceLabel;
    out->append(entry);
}

QStringList ProtonManager::steamRoots() const
{
#if !defined(Q_OS_LINUX)
    return {};
#else
    QStringList roots;
    // Prefer installs that have the Steam client / overlay libs (not an empty
    // ~/.local/share/Steam that only holds compatibilitytools.d).
    const QStringList candidates = {
        QDir::homePath() + QStringLiteral("/.steam/steam"),
        QDir::homePath() + QStringLiteral("/.steam/root"),
        QDir::homePath() + QStringLiteral("/.steam/debian-installation"),
        QDir::homePath() + QStringLiteral("/.local/share/Steam"),
        QDir::homePath()
        + QStringLiteral("/.var/app/com.valvesoftware.Steam/.local/share/Steam"),
        QDir::homePath()
        + QStringLiteral("/.var/app/com.valvesoftware.Steam/data/Steam"),
    };

    QStringList weak;
    for (const QString& candidate : candidates) {
        const QString normalized = normalizePath(candidate);
        if (normalized.isEmpty() || !QDir(normalized).exists() || roots.contains(normalized)
            || weak.contains(normalized))
            continue;
        const bool hasClient =
            QFileInfo::exists(normalized + QStringLiteral("/ubuntu12_64/gameoverlayrenderer.so"))
            || QFileInfo::exists(normalized + QStringLiteral("/steam.sh"))
            || QFileInfo::exists(normalized + QStringLiteral("/ubuntu12_32/steam"));
        if (hasClient)
            roots.append(normalized);
        else
            weak.append(normalized);
    }
    for (const QString& path : weak) {
        if (!roots.contains(path))
            roots.append(path);
    }
    return roots;
#endif
}

QStringList ProtonManager::steamLibraryRoots(const QString& steamRoot) const
{
    QStringList libraries;
    const QString normalizedRoot = normalizePath(steamRoot);
    if (normalizedRoot.isEmpty())
        return libraries;

    libraries.append(normalizedRoot);

    const QStringList vdfPaths = {
        normalizedRoot + QStringLiteral("/steamapps/libraryfolders.vdf"),
        normalizedRoot + QStringLiteral("/config/libraryfolders.vdf"),
    };

    for (const QString& vdfPath : vdfPaths) {
        QFile file(vdfPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        const QString content = QString::fromUtf8(file.readAll());
        QRegularExpression pathRe(QStringLiteral("\"path\"\\s+\"([^\"]+)\""));
        auto it = pathRe.globalMatch(content);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            QString path = match.captured(1);
            path.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
            const QString normalized = normalizePath(path);
            if (!normalized.isEmpty() && !libraries.contains(normalized))
                libraries.append(normalized);
        }

        QRegularExpression legacyRe(QStringLiteral("\"\\d+\"\\s+\"([^\"]+)\""));
        auto legacyIt = legacyRe.globalMatch(content);
        while (legacyIt.hasNext()) {
            const QRegularExpressionMatch match = legacyIt.next();
            const QString normalized = normalizePath(match.captured(1));
            if (!normalized.isEmpty() && !libraries.contains(normalized))
                libraries.append(normalized);
        }
    }

    return libraries;
}

void ProtonManager::scanEntries(QVector<ProtonEntry>* out) const
{
    if (!out)
        return;

    out->clear();

    QDir arachnelRoot(protonInstallRoot());
    const QStringList arachnelDirs =
        arachnelRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
    for (const QString& dirName : arachnelDirs) {
        if (isForeignArchGeProton(dirName))
            continue;
        appendEntry(out, QStringLiteral("arachnel"), QStringLiteral("Arachnel"),
                    arachnelRoot.filePath(dirName), dirName);
    }

#if defined(Q_OS_LINUX)
    for (const QString& steamRoot : steamRoots()) {
        for (const QString& libraryRoot : steamLibraryRoots(steamRoot)) {
            QDir toolsDir(libraryRoot + QStringLiteral("/compatibilitytools.d"));
            if (toolsDir.exists()) {
                const QStringList toolDirs =
                    toolsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot,
                                       QDir::Name | QDir::Reversed);
                for (const QString& toolDir : toolDirs) {
                    appendEntry(out, QStringLiteral("steam-tool"), QStringLiteral("Steam"),
                                toolsDir.filePath(toolDir), toolDir);
                }
            }

            QDir commonDir(libraryRoot + QStringLiteral("/steamapps/common"));
            if (!commonDir.exists())
                continue;

            const QStringList protonDirs =
                commonDir.entryList(QStringList{QStringLiteral("Proton*")}, QDir::Dirs,
                                    QDir::Name | QDir::Reversed);
            for (const QString& protonDir : protonDirs) {
                appendEntry(out, QStringLiteral("steam"), QStringLiteral("Steam"),
                            commonDir.filePath(protonDir), protonDir);
            }
        }
    }
#endif
}

void ProtonManager::invalidateScanCache()
{
    m_cacheValid = false;
    emit availableEntriesChanged();
}

QVector<ProtonEntry> ProtonManager::availableEntries(bool forceRescan) const
{
    if (forceRescan || !m_cacheValid) {
        scanEntries(&m_cachedEntries);
        m_cacheValid = true;
    }
    return m_cachedEntries;
}

QStringList ProtonManager::installedVersions() const
{
    QStringList versions;
    for (const ProtonEntry& entry : availableEntries()) {
        if (entry.source == QStringLiteral("arachnel"))
            versions.append(entry.name);
    }
    return versions;
}

QString ProtonManager::executableForId(const QString& id) const
{
    if (id.trimmed().isEmpty())
        return {};

    for (const ProtonEntry& entry : availableEntries()) {
        if (entry.id == id)
            return findProtonScriptInDir(entry.installDir);
    }
    return {};
}

QString ProtonManager::installDirForId(const QString& id) const
{
    if (id.trimmed().isEmpty())
        return {};

    for (const ProtonEntry& entry : availableEntries()) {
        if (entry.id == id)
            return entry.installDir;
    }
    return {};
}

QString ProtonManager::nameForId(const QString& id) const
{
    if (id.trimmed().isEmpty())
        return {};

    for (const ProtonEntry& entry : availableEntries()) {
        if (entry.id == id)
            return entry.name;
    }
    return {};
}

QString ProtonManager::idForInstallDir(const QString& installDir) const
{
    const QString normalized = normalizePath(installDir);
    if (normalized.isEmpty())
        return {};

    for (const ProtonEntry& entry : availableEntries()) {
        if (entry.installDir == normalized)
            return entry.id;
    }

    const QFileInfo info(normalized);
    if (info.fileName() == QStringLiteral("proton") && info.isFile()) {
        const QString parentDir = info.absolutePath();
        for (const ProtonEntry& entry : availableEntries()) {
            if (entry.installDir == parentDir)
                return entry.id;
        }
    }

    return {};
}

QString ProtonManager::resolveProtonId(const QString& gameProtonId,
                                         const QString& defaultProtonId,
                                         const QStringList& priorityIds) const
{
    const auto tryId = [this](const QString& id) -> QString {
        const QString trimmed = id.trimmed();
        if (trimmed.isEmpty())
            return {};
        return executableForId(trimmed).isEmpty() ? QString() : trimmed;
    };

    if (const QString resolved = tryId(gameProtonId); !resolved.isEmpty())
        return resolved;

    if (const QString resolved = tryId(defaultProtonId); !resolved.isEmpty())
        return resolved;

    for (const QString& id : priorityIds) {
        if (const QString resolved = tryId(id); !resolved.isEmpty())
            return resolved;
    }

    const QVector<ProtonEntry> entries = availableEntries();
    return entries.isEmpty() ? QString() : entries.first().id;
}

QString ProtonManager::resolveProtonExecutable(const QString& preferredIdOrLegacyPath) const
{
    const QString preferred = preferredIdOrLegacyPath.trimmed();
    if (!preferred.isEmpty()) {
        const QString byId = executableForId(preferred);
        if (!byId.isEmpty())
            return byId;

        QFileInfo legacy(preferred);
        if (legacy.isDir()) {
            const QString script = findProtonScriptInDir(legacy.absoluteFilePath());
            if (!script.isEmpty())
                return script;
        } else if (legacy.exists() && legacy.isExecutable()) {
            return legacy.absoluteFilePath();
        }

        const QString mappedId = idForInstallDir(preferred);
        const QString mapped = executableForId(mappedId);
        if (!mapped.isEmpty())
            return mapped;
    }

    const QVector<ProtonEntry> entries = availableEntries();
    if (!entries.isEmpty())
        return findProtonScriptInDir(entries.first().installDir);

    return {};
}

QString ProtonManager::activeVersionName(const QString& preferredIdOrLegacyPath) const
{
    const QString preferred = preferredIdOrLegacyPath.trimmed();
    if (!preferred.isEmpty()) {
        const QString name = nameForId(preferred);
        if (!name.isEmpty())
            return name;

        const QFileInfo info(preferred);
        const QString parentName = info.absoluteDir().dirName();
        if (parentName.startsWith(QStringLiteral("GE-Proton"))
            || parentName.startsWith(QStringLiteral("Proton")))
            return parentName;
    }

    const QVector<ProtonEntry> entries = availableEntries();
    return entries.isEmpty() ? QString() : entries.first().name;
}

bool ProtonManager::isAvailable(const QString& preferredIdOrLegacyPath) const
{
    return !resolveProtonExecutable(preferredIdOrLegacyPath).isEmpty();
}

QString ProtonManager::steamCompatClientPath() const
{
#if defined(Q_OS_LINUX)
    const QStringList roots = steamRoots();
    if (!roots.isEmpty())
        return roots.first();

    const QString shim = appDataDir() + QStringLiteral("/steam-shim");
    QDir().mkpath(shim + QStringLiteral("/steamapps/common"));
    QDir().mkpath(shim + QStringLiteral("/compatibilitytools.d"));
    return shim;
#else
    return {};
#endif
}

int ProtonManager::repairCorruptPrefixForGame(const QString& gameId) const
{
#if !defined(Q_OS_LINUX)
    Q_UNUSED(gameId);
    return 0;
#else
    const QString safeId = gameId.trimmed().isEmpty() ? QStringLiteral("default") : gameId;
    const QString windowsDir =
        compatDataRoot() + QLatin1Char('/') + safeId + QStringLiteral("/pfx/drive_c/windows");
    int repaired = 0;

    // Proton creates these as directories. A previous launcher/runtime copy bug could
    // leave a DLL at the directory path (notably openvr_api_dxvk.dll at `syswow64`),
    // making Proton abort before it can start the game. Preserve the bad file for
    // diagnosis, then let Proton recreate the directory and its builtins.
    for (const QString& name : {QStringLiteral("system32"), QStringLiteral("syswow64")}) {
        const QString path = QDir(windowsDir).filePath(name);
        const QFileInfo info(path);
        if (!info.exists() || !info.isFile())
            continue;

        QString backup = path + QStringLiteral(".arachnel-corrupt");
        if (QFileInfo::exists(backup)) {
            int suffix = 2;
            do {
                backup = path + QStringLiteral(".arachnel-corrupt.%1").arg(suffix++);
            } while (QFileInfo::exists(backup));
        }

        if (QFile::rename(path, backup) || QFile::remove(path))
            ++repaired;
    }
    return repaired;
#endif
}

QString ProtonManager::repairLegacyPrefixVersionForGame(const QString& gameId,
                                                         const QString& protonVersion) const
{
#if !defined(Q_OS_LINUX)
    Q_UNUSED(gameId);
    Q_UNUSED(protonVersion);
    return {};
#else
    const QString targetVersion = protonVersion.trimmed();
    if (targetVersion.isEmpty())
        return {};

    const QString safeId = gameId.trimmed().isEmpty() ? QStringLiteral("default") : gameId;
    const QString root = compatDataRoot() + QLatin1Char('/') + safeId;
    const QString versionPath = root + QStringLiteral("/version");
    QFile versionFile(versionPath);
    if (!versionFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    const QString oldVersion = QString::fromUtf8(versionFile.readAll()).trimmed();
    versionFile.close();
    if (oldVersion.isEmpty() || oldVersion == targetVersion
        || !oldVersion.startsWith(QStringLiteral("CachyOS-"), Qt::CaseInsensitive))
        return {};

    // GE-Proton expects major.minor-...; older CachyOS wrote CachyOS-11.0-100.
    QString backupPath = versionPath + QStringLiteral(".arachnel-backup");
    if (QFileInfo::exists(backupPath)) {
        int suffix = 2;
        do {
            backupPath = versionPath + QStringLiteral(".arachnel-backup.%1").arg(suffix++);
        } while (QFileInfo::exists(backupPath));
    }
    if (!QFile::copy(versionPath, backupPath))
        return {};

    QSaveFile replacement(versionPath);
    if (!replacement.open(QIODevice::WriteOnly | QIODevice::Text)
        || replacement.write((targetVersion + QLatin1Char('\n')).toUtf8()) < 0
        || !replacement.commit()) {
        QFile::remove(versionPath);
        QFile::copy(backupPath, versionPath);
        return {};
    }

    return QCoreApplication::translate("Core",
                                       "Normalized legacy Proton prefix marker %1 -> %2 (backup: %3)")
        .arg(oldVersion, targetVersion, QFileInfo(backupPath).fileName());
#endif
}

QString ProtonManager::compatDataPathForGame(const QString& gameId) const
{
    const QString safeId = gameId.trimmed().isEmpty() ? QStringLiteral("default") : gameId;
    const QString path = compatDataRoot() + QLatin1Char('/') + safeId;
    QDir().mkpath(path);
    repairCorruptPrefixForGame(gameId);
    return path;
}

bool ProtonManager::steamLinuxRuntimeUsable() const
{
#if !defined(Q_OS_LINUX)
    return false;
#else
    // Ubuntu 23.10+: apparmor_restrict_unprivileged_userns=1 blocks bwrap userns for
    // apps without an AppArmor profile (Steam itself is exempt). Online Fix then fails
    // inside pressure-vessel; launch Proton directly instead.
    const auto readSysctl = [](const char* path) -> QByteArray {
        QFile file(QString::fromUtf8(path));
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return file.readAll().trimmed();
    };
    if (readSysctl("/proc/sys/kernel/apparmor_restrict_unprivileged_userns") == "1")
        return false;
    if (readSysctl("/proc/sys/kernel/unprivileged_userns_clone") == "0")
        return false;
    return true;
#endif
}

bool ProtonManager::canAaExecSteamProfile() const
{
#if !defined(Q_OS_LINUX)
    return false;
#else
    // Borrow Steam's AppArmor profile (userns allowed) so pressure-vessel can start.
    return QFileInfo::exists(QStringLiteral("/usr/bin/aa-exec"))
        && QFileInfo::exists(QStringLiteral("/etc/apparmor.d/steam"));
#endif
}

QString ProtonManager::findLegacySteamRuntime() const
{
#if !defined(Q_OS_LINUX)
    return {};
#else
    // SOFL online-fix-use-steam-runtime: <steam>/ubuntu12_32/steam-runtime/run.sh
    for (const QString& steamRoot : steamRoots()) {
        const QString runPath =
            steamRoot + QStringLiteral("/ubuntu12_32/steam-runtime/run.sh");
        if (QFileInfo::exists(runPath) && QFileInfo(runPath).isExecutable())
            return runPath;
    }
    return {};
#endif
}

QString ProtonManager::findSteamLinuxRuntime() const
{
#if !defined(Q_OS_LINUX)
    return {};
#else
    // Prefer Sniper (1628350), then Soldier — used only when explicitly requested.
    const QStringList runtimeNames = {
        QStringLiteral("SteamLinuxRuntime_sniper"),
        QStringLiteral("SteamLinuxRuntime_soldier"),
        QStringLiteral("SteamLinuxRuntime"),
    };
    for (const QString& steamRoot : steamRoots()) {
        for (const QString& libraryRoot : steamLibraryRoots(steamRoot)) {
            for (const QString& name : runtimeNames) {
                const QString runPath =
                    libraryRoot + QStringLiteral("/steamapps/common/") + name
                    + QStringLiteral("/run");
                if (QFileInfo::exists(runPath) && QFileInfo(runPath).isExecutable())
                    return runPath;
            }
        }
    }
    return {};
#endif
}

} // namespace arachnel::core

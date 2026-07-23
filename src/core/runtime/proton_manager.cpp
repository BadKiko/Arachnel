#include "proton_manager.h"

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
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QEventLoop>

namespace arachnel::core {


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
    // Prefer paths SOFL / Steam client use for COMPAT_CLIENT + overlay libs.
    const QStringList candidates = {
        QDir::homePath() + QStringLiteral("/.steam/steam"),
        QDir::homePath() + QStringLiteral("/.steam/root"),
        QDir::homePath() + QStringLiteral("/.local/share/Steam"),
        QDir::homePath()
        + QStringLiteral("/.var/app/com.valvesoftware.Steam/.local/share/Steam"),
        QDir::homePath()
        + QStringLiteral("/.var/app/com.valvesoftware.Steam/data/Steam"),
    };

    for (const QString& candidate : candidates) {
        const QString normalized = normalizePath(candidate);
        if (!normalized.isEmpty() && QDir(normalized).exists() && !roots.contains(normalized))
            roots.append(normalized);
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

QString ProtonManager::compatDataPathForGame(const QString& gameId) const
{
    const QString safeId = gameId.trimmed().isEmpty() ? QStringLiteral("default") : gameId;
    const QString path = compatDataRoot() + QLatin1Char('/') + safeId;
    QDir().mkpath(path);
    return path;
}

QString ProtonManager::findSteamLinuxRuntime() const
{
#if !defined(Q_OS_LINUX)
    return {};
#else
    // Prefer Sniper (1628350), then Soldier — same order SOFL looks for.
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

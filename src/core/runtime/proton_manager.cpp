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

namespace {

QString appDataDir()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir;
}

QString normalizePath(const QString& path)
{
    return QFileInfo(path).canonicalFilePath().isEmpty() ? QFileInfo(path).absoluteFilePath()
                                                           : QFileInfo(path).canonicalFilePath();
}

} // namespace

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
    const QStringList candidates = {
        QDir::homePath() + QStringLiteral("/.steam/root"),
        QDir::homePath() + QStringLiteral("/.local/share/Steam"),
        QDir::homePath()
        + QStringLiteral("/.var/app/com.valvesoftware.Steam/.local/share/Steam"),
    };

    for (const QString& candidate : candidates) {
        const QString normalized = normalizePath(candidate);
        if (!normalized.isEmpty() && !roots.contains(normalized))
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

bool ProtonManager::fetchLatestGeReleaseInfo(QString* versionNameOut, QString* downloadUrlOut)
{
    if (!versionNameOut || !downloadUrlOut)
        return false;

    auto* network = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl(QStringLiteral(
        "https://api.github.com/repos/GloriousEggroll/proton-ge-custom/releases/latest")));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel"));

    QEventLoop loop;
    bool ok = false;
    QNetworkReply* reply = network->get(request);
    connect(reply, &QNetworkReply::finished, this, [&]() {
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
            for (const QJsonValue& value : release.value(QStringLiteral("assets")).toArray()) {
                const QJsonObject asset = value.toObject();
                const QString name = asset.value(QStringLiteral("name")).toString();
                if (name.startsWith(QStringLiteral("GE-Proton"))
                    && name.endsWith(QStringLiteral(".tar.gz"), Qt::CaseInsensitive)
                    && !name.contains(QStringLiteral("sha512"), Qt::CaseInsensitive)) {
                    *downloadUrlOut = asset.value(QStringLiteral("browser_download_url")).toString();
                    *versionNameOut =
                        name.left(name.size() - QStringLiteral(".tar.gz").size());
                    ok = true;
                    break;
                }
            }
        }
        loop.quit();
    });
    loop.exec();
    reply->deleteLater();
    network->deleteLater();
    return ok;
}

void ProtonManager::refreshLatestGeRelease()
{
#if !defined(Q_OS_LINUX)
    return;
#else
    auto* network = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl(QStringLiteral(
        "https://api.github.com/repos/GloriousEggroll/proton-ge-custom/releases/latest")));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel"));
    QNetworkReply* reply = network->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, network, reply]() {
        reply->deleteLater();
        network->deleteLater();

        if (reply->error() != QNetworkReply::NoError)
            return;

        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        for (const QJsonValue& value : release.value(QStringLiteral("assets")).toArray()) {
            const QJsonObject asset = value.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            if (name.startsWith(QStringLiteral("GE-Proton"))
                && name.endsWith(QStringLiteral(".tar.gz"), Qt::CaseInsensitive)
                && !name.contains(QStringLiteral("sha512"), Qt::CaseInsensitive)) {
                const QString versionName =
                    name.left(name.size() - QStringLiteral(".tar.gz").size());
                if (m_latestGeReleaseName != versionName) {
                    m_latestGeReleaseName = versionName;
                    emit latestGeReleaseChanged();
                }
                return;
            }
        }
    });
#endif
}

void ProtonManager::setDownloadProgress(int percent, const QString& status)
{
    m_downloadProgress = qBound(0, percent, 100);
    m_downloadStatus = status;
    emit downloadStateChanged();
}

void ProtonManager::finishDownload(bool success, const QString& error)
{
    m_downloading = false;
    if (success) {
        invalidateScanCache();
        emit versionsChanged();
    }
    emit downloadStateChanged();
    emit downloadFinished(success, error);
}

bool ProtonManager::extractTarGz(const QString& archivePath, const QString& destDir,
                                 QString* errorOut)
{
    QProcess process;
    process.setProgram(QStringLiteral("tar"));
    process.setArguments({QStringLiteral("-xzf"), archivePath, QStringLiteral("-C"), destDir});
    process.start();
    if (!process.waitForStarted(5000)) {
        if (errorOut)
            *errorOut = QStringLiteral("tar failed to start");
        return false;
    }
    if (!process.waitForFinished(-1)) {
        if (errorOut)
            *errorOut = QStringLiteral("tar extraction timed out");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorOut)
            *errorOut = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return false;
    }
    return true;
}

void ProtonManager::downloadLatestGe()
{
#if !defined(Q_OS_LINUX)
    finishDownload(false, QStringLiteral("Proton-GE is only available on Linux"));
    return;
#else
    if (m_downloading)
        return;

    m_downloading = true;
    m_downloadProgress = 0;
    m_downloadStatus = QStringLiteral("Fetching release info…");
    emit downloadStateChanged();

    QString versionName;
    QString downloadUrl;
    if (!fetchLatestGeReleaseInfo(&versionName, &downloadUrl)) {
        finishDownload(false, QStringLiteral("No Proton-GE archive found in latest release"));
        return;
    }

    if (!m_latestGeReleaseName.isEmpty() && m_latestGeReleaseName != versionName) {
        m_latestGeReleaseName = versionName;
        emit latestGeReleaseChanged();
    } else if (m_latestGeReleaseName.isEmpty()) {
        m_latestGeReleaseName = versionName;
        emit latestGeReleaseChanged();
    }

    const QString assetName = versionName + QStringLiteral(".tar.gz");
    const QString tempDir = appDataDir() + QStringLiteral("/proton-download");
    QDir().mkpath(tempDir);
    const QString archivePath = tempDir + QLatin1Char('/') + assetName;

    setDownloadProgress(0, QStringLiteral("Downloading %1…").arg(versionName));

    auto* downloadNetwork = new QNetworkAccessManager(this);
    QNetworkRequest downloadRequest{QUrl(downloadUrl)};
    downloadRequest.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel"));
    QNetworkReply* downloadReply = downloadNetwork->get(downloadRequest);

    connect(downloadReply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                if (total <= 0)
                    return;
                const int percent = static_cast<int>((received * 80) / total);
                setDownloadProgress(percent, QStringLiteral("Downloading Proton-GE…"));
            });

    connect(downloadReply, &QNetworkReply::finished, this,
            [this, downloadNetwork, downloadReply, archivePath, versionName]() {
                downloadReply->deleteLater();
                downloadNetwork->deleteLater();

                if (downloadReply->error() != QNetworkReply::NoError) {
                    finishDownload(false, downloadReply->errorString());
                    return;
                }

                QFile file(archivePath);
                if (!file.open(QIODevice::WriteOnly)) {
                    finishDownload(false, file.errorString());
                    return;
                }
                file.write(downloadReply->readAll());
                file.close();

                setDownloadProgress(85, QStringLiteral("Extracting…"));
                const QString destDir = protonInstallRoot() + QLatin1Char('/') + versionName;
                QDir().mkpath(destDir);

                QString extractError;
                if (!extractTarGz(archivePath, destDir, &extractError)) {
                    finishDownload(false, extractError);
                    return;
                }

                QFile::remove(archivePath);
                const QString protonScript = findProtonScriptInDir(destDir);
                if (protonScript.isEmpty()) {
                    finishDownload(false, QStringLiteral("Proton script not found after extraction"));
                    return;
                }

                setDownloadProgress(100, QStringLiteral("Proton-GE installed"));
                finishDownload(true);
            });
#endif
}

} // namespace arachnel::core

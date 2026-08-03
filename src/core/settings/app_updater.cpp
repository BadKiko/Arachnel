#include "app_updater.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QVersionNumber>

#include <utility>

namespace arachnel::core {

namespace {

const char* kGithubLatestRelease =
    "https://api.github.com/repos/BadKiko/Arachnel/releases/latest";
const char* kGithubReleasesList =
    "https://api.github.com/repos/BadKiko/Arachnel/releases?per_page=30";
const char* kGithubReleasesPage = "https://github.com/BadKiko/Arachnel/releases/latest";

QString preferredAssetNameHint()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("Setup.exe");
#elif defined(Q_OS_LINUX)
    return QStringLiteral(".AppImage");
#else
    return {};
#endif
}

} // namespace

AppUpdater::AppUpdater(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_releasePageUrl = QString::fromUtf8(kGithubReleasesPage);
    m_statusText = QCoreApplication::translate("Core", "Not checked yet");
}

QString AppUpdater::currentVersion() const
{
    return normalizeVersion(QCoreApplication::applicationVersion());
}

void AppUpdater::setIncludePreReleases(bool enabled)
{
    if (m_includePreReleases == enabled)
        return;
    m_includePreReleases = enabled;
    emit includePreReleasesChanged();
    // Channel change invalidates the last check - refresh so UI picks up pre-releases.
    if (!m_checking && !m_downloading)
        checkForUpdates(false);
}

QString AppUpdater::normalizeVersion(const QString& version)
{
    QString value = version.trimmed();
    if (value.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        value.remove(0, 1);
    return value;
}

int AppUpdater::compareVersions(const QString& left, const QString& right)
{
    return compareVersionParts(left, right, /*letteredNewer=*/true);
}

int AppUpdater::compareVersionsPreferPlain(const QString& left, const QString& right)
{
    // Stable channel: plain 0.1.38 is preferred over lettered 0.1.38a (leave pre builds).
    return compareVersionParts(left, right, /*letteredNewer=*/false);
}

int AppUpdater::compareVersionParts(const QString& left, const QString& right, bool letteredNewer)
{
    const QString a = normalizeVersion(left);
    const QString b = normalizeVersion(right);
    if (a == QLatin1String("dev") && b != QLatin1String("dev"))
        return -1;
    if (b == QLatin1String("dev") && a != QLatin1String("dev"))
        return 1;

    const auto parse = [](const QString& value) {
        int i = 0;
        while (i < value.size()) {
            const QChar c = value.at(i);
            if (c.isDigit() || c == QLatin1Char('.')) {
                ++i;
                continue;
            }
            break;
        }
        QVersionNumber number = QVersionNumber::fromString(value.left(i));
        QString suffix = value.mid(i).trimmed();
        if (suffix.startsWith(QLatin1Char('-')) || suffix.startsWith(QLatin1Char('+')))
            suffix.remove(0, 1);
        return std::pair<QVersionNumber, QString>{number, suffix.toLower()};
    };

    const auto leftVersion = parse(a);
    const auto rightVersion = parse(b);
    if (!leftVersion.first.isNull() && !rightVersion.first.isNull()) {
        const int numberCmp = QVersionNumber::compare(leftVersion.first, rightVersion.first);
        if (numberCmp != 0)
            return numberCmp;
        // Same numeric core (0.1.38 / 0.1.38a / 0.1.38b).
        // Pre channel: plain < a < b (lettered hotfixes after stable).
        // Stable channel: plain > lettered (move off pre builds onto plain tag).
        if (leftVersion.second.isEmpty() && !rightVersion.second.isEmpty())
            return letteredNewer ? -1 : 1;
        if (!leftVersion.second.isEmpty() && rightVersion.second.isEmpty())
            return letteredNewer ? 1 : -1;
        return QString::compare(leftVersion.second, rightVersion.second, Qt::CaseInsensitive);
    }

    return QString::compare(a, b, Qt::CaseInsensitive);
}

void AppUpdater::setChecking(bool value)
{
    if (m_checking == value)
        return;
    m_checking = value;
    emit stateChanged();
}

void AppUpdater::setDownloading(bool value)
{
    if (m_downloading == value)
        return;
    m_downloading = value;
    emit stateChanged();
}

void AppUpdater::setStatusText(const QString& text)
{
    if (m_statusText == text)
        return;
    m_statusText = text;
    emit stateChanged();
}

void AppUpdater::setLastError(const QString& error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit stateChanged();
}

void AppUpdater::checkForUpdates(bool notifyIfUpToDate)
{
    if (currentVersion().compare(QStringLiteral("dev"), Qt::CaseInsensitive) == 0) {
        setChecking(false);
        setStatusText(QCoreApplication::translate("Core", "Dev build - app updates disabled"));
        emit updateCheckFinished(false, {});
        return;
    }
    if (m_checking || m_downloading)
        return;

    setLastError({});
    setChecking(true);
    setStatusText(QCoreApplication::translate("Core", "Checking for Arachnel updates…"));

    const QUrl apiUrl(QString::fromUtf8(m_includePreReleases ? kGithubReleasesList
                                                             : kGithubLatestRelease));
    QNetworkRequest request(apiUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Arachnel/%1").arg(currentVersion()));
    request.setRawHeader("Accept", "application/vnd.github+json");

    if (m_activeReply) {
        QObject::disconnect(m_activeReply, nullptr, this, nullptr);
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }

    QNetworkReply* reply = m_network->get(request);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, notifyIfUpToDate]() {
        if (m_activeReply == reply)
            m_activeReply = nullptr;
        reply->deleteLater();
        setChecking(false);

        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() == QNetworkReply::OperationCanceledError)
                return;
            const QString error = QCoreApplication::translate("Core", "Update check failed: %1")
                                      .arg(reply->errorString());
            setLastError(error);
            setStatusText(error);
            emit updateFailed(error);
            return;
        }

        const QByteArray payload = reply->readAll();
        if (m_includePreReleases)
            handleReleasesListPayload(payload, notifyIfUpToDate);
        else
            handleReleasePayload(payload, notifyIfUpToDate);
    });
}

QString AppUpdater::assetDownloadUrl(const QJsonObject& release)
{
    const QString hint = preferredAssetNameHint();
    for (const QJsonValue& value : release.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        const QString url = asset.value(QStringLiteral("browser_download_url")).toString();
        if (name.isEmpty() || url.isEmpty())
            continue;
        if (!hint.isEmpty() && name.contains(hint, Qt::CaseInsensitive))
            return url;
    }
    return {};
}

void AppUpdater::handleReleasesListPayload(const QByteArray& payload, bool notifyIfUpToDate)
{
    const QJsonArray releases = QJsonDocument::fromJson(payload).array();
    if (releases.isEmpty()) {
        const QString error =
            QCoreApplication::translate("Core", "Could not parse GitHub release information");
        setLastError(error);
        setStatusText(error);
        emit updateFailed(error);
        return;
    }

    // Newest publish wins. Needed because lettered tags (0.1.38a) can ship after the
    // plain stable (0.1.38), and semver-style "plain > letter" would hide pre-releases.
    QJsonObject best;
    QDateTime bestPublished;
    for (const QJsonValue& value : releases) {
        if (!value.isObject())
            continue;
        const QJsonObject release = value.toObject();
        if (release.value(QStringLiteral("draft")).toBool(false))
            continue;
        if (!m_includePreReleases && release.value(QStringLiteral("prerelease")).toBool(false))
            continue;
        if (assetDownloadUrl(release).isEmpty())
            continue;
        const QString tag = normalizeVersion(release.value(QStringLiteral("tag_name")).toString());
        if (tag.isEmpty())
            continue;

        QDateTime published =
            QDateTime::fromString(release.value(QStringLiteral("published_at")).toString(),
                                  Qt::ISODate);
        if (!published.isValid()) {
            published = QDateTime::fromString(
                release.value(QStringLiteral("created_at")).toString(), Qt::ISODate);
        }
        if (!published.isValid())
            published = QDateTime::fromSecsSinceEpoch(0);

        const bool take = best.isEmpty() || published > bestPublished
                          || (published == bestPublished
                              && compareVersions(best.value(QStringLiteral("tag_name")).toString(),
                                                 tag)
                                     < 0);
        if (take) {
            best = release;
            bestPublished = published;
        }
    }

    if (best.isEmpty()) {
        const QString error =
            QCoreApplication::translate("Core", "Could not parse GitHub release information");
        setLastError(error);
        setStatusText(error);
        emit updateFailed(error);
        return;
    }

    handleReleaseObject(best, notifyIfUpToDate);
}

void AppUpdater::handleReleasePayload(const QByteArray& payload, bool notifyIfUpToDate)
{
    handleReleaseObject(QJsonDocument::fromJson(payload).object(), notifyIfUpToDate);
}

void AppUpdater::handleReleaseObject(const QJsonObject& release, bool notifyIfUpToDate)
{
    const QString tag = normalizeVersion(release.value(QStringLiteral("tag_name")).toString());
    const QString htmlUrl = release.value(QStringLiteral("html_url")).toString();
    if (!htmlUrl.isEmpty())
        m_releasePageUrl = htmlUrl;
    else
        m_releasePageUrl = QString::fromUtf8(kGithubReleasesPage);

    if (tag.isEmpty()) {
        const QString error =
            QCoreApplication::translate("Core", "Could not parse GitHub release information");
        setLastError(error);
        setStatusText(error);
        emit updateFailed(error);
        return;
    }

    m_latestVersion = tag;
    m_downloadUrl = assetDownloadUrl(release);

    const int cmp = m_includePreReleases ? compareVersions(currentVersion(), tag)
                                         : compareVersionsPreferPlain(currentVersion(), tag);
    const bool available = cmp < 0 && !m_downloadUrl.isEmpty();
    m_updateAvailable = available;

    if (available) {
        if (release.value(QStringLiteral("prerelease")).toBool(false)) {
            setStatusText(
                QCoreApplication::translate("Core", "Arachnel %1 (pre-release) is available")
                    .arg(tag));
        } else {
            setStatusText(QCoreApplication::translate("Core", "Arachnel %1 is available").arg(tag));
        }
    } else if (cmp >= 0) {
        setStatusText(QCoreApplication::translate("Core", "Arachnel is up to date (%1)")
                          .arg(currentVersion()));
        Q_UNUSED(notifyIfUpToDate);
    } else {
        setStatusText(QCoreApplication::translate(
            "Core", "Update found, but no installer package is available for this platform"));
    }

    emit stateChanged();
    emit updateCheckFinished(available, tag);
}

void AppUpdater::downloadAndInstall()
{
    if (m_downloading || m_checking)
        return;
    if (!m_updateAvailable || m_downloadUrl.isEmpty()) {
        checkForUpdates(true);
        return;
    }

#if !defined(Q_OS_WIN)
    openReleasePage();
    setStatusText(QCoreApplication::translate(
        "Core", "Open the release page to download the latest package for your platform"));
    return;
#else
    startDownload(QUrl(m_downloadUrl));
#endif
}

void AppUpdater::startDownload(const QUrl& url)
{
    setLastError({});
    setDownloading(true);
    m_downloadProgress = 0;
    m_downloadBytesTotal = 0;
    emit downloadProgressChanged();
    setStatusText(QCoreApplication::translate("Core", "Downloading Arachnel update…"));

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tempDir);
    const QString fileName =
        QStringLiteral("Arachnel-%1-Setup.exe").arg(m_latestVersion.isEmpty()
                                                        ? QStringLiteral("update")
                                                        : m_latestVersion);
    const QString targetPath = QDir(tempDir).absoluteFilePath(fileName);
    QFile::remove(targetPath);

    auto* outFile = new QFile(targetPath, this);
    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        outFile->deleteLater();
        setDownloading(false);
        const QString error =
            QCoreApplication::translate("Core", "Could not save the downloaded installer");
        setLastError(error);
        setStatusText(error);
        emit updateFailed(error);
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Arachnel/%1").arg(currentVersion()));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    if (m_activeReply) {
        QObject::disconnect(m_activeReply, nullptr, this, nullptr);
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }

    QNetworkReply* reply = m_network->get(request);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, reply](qint64 received, qint64 total) {
                if (m_activeReply != reply)
                    return;
                m_downloadBytesTotal = total;
                if (total > 0)
                    m_downloadProgress = static_cast<int>((received * 100) / total);
                else
                    m_downloadProgress = 0;
                emit downloadProgressChanged();
            });
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, outFile]() {
        if (m_activeReply != reply || !outFile)
            return;
        outFile->write(reply->readAll());
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, outFile, targetPath]() {
        if (m_activeReply == reply)
            m_activeReply = nullptr;

        outFile->write(reply->readAll());
        outFile->close();
        outFile->deleteLater();
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QFile::remove(targetPath);
            setDownloading(false);
            if (reply->error() == QNetworkReply::OperationCanceledError)
                return;
            const QString error = QCoreApplication::translate("Core", "Download failed: %1")
                                      .arg(reply->errorString());
            setLastError(error);
            setStatusText(error);
            emit updateFailed(error);
            return;
        }

        if (QFileInfo(targetPath).size() <= 0) {
            QFile::remove(targetPath);
            setDownloading(false);
            const QString error =
                QCoreApplication::translate("Core", "Could not save the downloaded installer");
            setLastError(error);
            setStatusText(error);
            emit updateFailed(error);
            return;
        }

        m_downloadProgress = 100;
        emit downloadProgressChanged();
        setStatusText(QCoreApplication::translate("Core", "Updating Arachnel…"));

        QString launchError;
        if (!launchInstaller(targetPath, &launchError)) {
            setDownloading(false);
            setLastError(launchError);
            setStatusText(launchError);
            emit updateFailed(launchError);
            return;
        }

        emit installerLaunchRequested();
    });
}

namespace {

#if defined(Q_OS_WIN)
QString readUninstallInstallLocation(const QString& uninstallKey)
{
    const QString valueName = QStringLiteral("InstallLocation");
    for (const QString& root : {
             QStringLiteral("HKEY_LOCAL_MACHINE\\%1").arg(uninstallKey),
             QStringLiteral("HKEY_CURRENT_USER\\%1").arg(uninstallKey),
         }) {
        QSettings reg(root, QSettings::NativeFormat);
        const QString loc = QDir::toNativeSeparators(reg.value(valueName).toString().trimmed());
        if (!loc.isEmpty())
            return QDir::cleanPath(loc);
    }
    return {};
}

QString resolveUpdateInstallDir(const QString& runningAppDir)
{
    const QString running = QDir::toNativeSeparators(QDir::cleanPath(runningAppDir));
    const QString runningLower = running.toLower();
    const bool unpackaged =
        runningLower.contains(QLatin1String("build-win"))
        || runningLower.contains(QLatin1String("\\build\\"))
        || runningLower.contains(QLatin1String("/build/"))
        || runningLower.contains(QLatin1String("relwithdebinfo"))
        || runningLower.contains(QLatin1String("\\debug"))
        || runningLower.contains(QLatin1String("/debug"));

    if (!unpackaged && QFileInfo::exists(running + QLatin1String("/arachnel_app.exe")))
        return running;

    // Inno AppId uninstall key (per-user lowest privileges).
    const QString innoDir = readUninstallInstallLocation(QStringLiteral(
        "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
        "{A8E3C1B2-4D5F-6A70-8B9C-0D1E2F3A4B5C}_is1"));
    if (!innoDir.isEmpty()
        && QFileInfo::exists(innoDir + QLatin1String("/arachnel_app.exe")))
        return innoDir;

    // Legacy Qt SFX uninstall key.
    const QString legacyDir = readUninstallInstallLocation(
        QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Arachnel"));
    if (!legacyDir.isEmpty()
        && QFileInfo::exists(legacyDir + QLatin1String("/arachnel_app.exe")))
        return legacyDir;

    // Default per-user install location used by Inno ({localappdata}\Programs\Arachnel).
    const QString localAppData = QString::fromLocal8Bit(qgetenv("LOCALAPPDATA"));
    const QString perUser = QDir::toNativeSeparators(QDir::cleanPath(
        localAppData.isEmpty()
            ? (QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
               + QStringLiteral("/Programs/Arachnel"))
            : (localAppData + QStringLiteral("/Programs/Arachnel"))));
    if (QFileInfo::exists(perUser + QLatin1String("/arachnel_app.exe")))
        return perUser;

    if (!innoDir.isEmpty())
        return innoDir;
    if (!legacyDir.isEmpty())
        return legacyDir;
    if (!unpackaged)
        return running;
    return perUser;
}

bool pathNeedsAllUsers(const QString& dir)
{
    const QString native = QDir::toNativeSeparators(dir);
    const auto underEnvRoot = [&native](const char* envName) {
        const QByteArray raw = qgetenv(envName);
        if (raw.isEmpty())
            return false;
        const QString root = QDir::toNativeSeparators(QString::fromLocal8Bit(raw));
        return !root.isEmpty() && native.startsWith(root, Qt::CaseInsensitive);
    };
    return underEnvRoot("ProgramFiles") || underEnvRoot("ProgramFiles(x86)")
           || underEnvRoot("ProgramW6432");
}
#endif

} // namespace

bool AppUpdater::launchInstaller(const QString& installerPath, QString* errorOut)
{
#if defined(Q_OS_WIN)
    const QString runningDir =
        QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    const QString targetDir = resolveUpdateInstallDir(runningDir);
    if (targetDir.isEmpty()) {
        if (errorOut) {
            *errorOut = QCoreApplication::translate(
                "Core", "Could not find an Arachnel install folder to update");
        }
        return false;
    }

    // In-app update: /SILENT shows unpack progress (no folder wizard), then [Run]
    // relaunches Arachnel (skipifnotsilent). Never /VERYSILENT - that hides progress.
    QStringList args = {
        QStringLiteral("/SILENT"),
        QStringLiteral("/NORESTART"),
        QStringLiteral("/CLOSEAPPLICATIONS"),
        QStringLiteral("/FORCECLOSEAPPLICATIONS"),
        QStringLiteral("/DIR=%1").arg(targetDir),
    };
    if (pathNeedsAllUsers(targetDir))
        args.append(QStringLiteral("/ALLUSERS"));
    // Older GitHub Setup builds (Qt SFX) only honor --update. Inno ignores unknown args.
    args.append(QStringLiteral("--update"));

    qint64 pid = 0;
    if (!QProcess::startDetached(installerPath, args, QFileInfo(installerPath).absolutePath(),
                                 &pid)
        || pid == 0) {
        if (errorOut) {
            *errorOut = QCoreApplication::translate("Core",
                                                    "Could not start the Arachnel installer");
        }
        return false;
    }
    qInfo().noquote() << "[app-updater] started installer pid" << pid << installerPath << args
                      << "target=" << targetDir;
    return true;
#else
    Q_UNUSED(installerPath);
    if (errorOut) {
        *errorOut = QCoreApplication::translate(
            "Core", "Automatic installer launch is only available on Windows");
    }
    return false;
#endif
}

void AppUpdater::openReleasePage()
{
    const QUrl url(m_releasePageUrl.isEmpty() ? QString::fromUtf8(kGithubReleasesPage)
                                              : m_releasePageUrl);
    QDesktopServices::openUrl(url);
}

} // namespace arachnel::core

#include "app_updater.h"

#include <QCoreApplication>
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
#include <QStandardPaths>
#include <QUrl>
#include <QVersionNumber>

namespace arachnel::core {

namespace {

const char* kGithubLatestRelease =
    "https://api.github.com/repos/BadKiko/Arachnel/releases/latest";
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

QString AppUpdater::normalizeVersion(const QString& version)
{
    QString value = version.trimmed();
    if (value.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        value.remove(0, 1);
    return value;
}

int AppUpdater::compareVersions(const QString& left, const QString& right)
{
    const QString a = normalizeVersion(left);
    const QString b = normalizeVersion(right);
    if (a == QLatin1String("dev") && b != QLatin1String("dev"))
        return -1;
    if (b == QLatin1String("dev") && a != QLatin1String("dev"))
        return 1;

    const QVersionNumber leftVersion = QVersionNumber::fromString(a);
    const QVersionNumber rightVersion = QVersionNumber::fromString(b);
    if (!leftVersion.isNull() && !rightVersion.isNull())
        return QVersionNumber::compare(leftVersion, rightVersion);

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
    if (m_checking || m_downloading)
        return;

    setLastError({});
    setChecking(true);
    setStatusText(QCoreApplication::translate("Core", "Checking for Arachnel updates…"));

    QNetworkRequest request(QUrl(QString::fromUtf8(kGithubLatestRelease)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Arachnel/%1").arg(currentVersion()));
    request.setRawHeader("Accept", "application/vnd.github+json");

    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }

    m_activeReply = m_network->get(request);
    connect(m_activeReply, &QNetworkReply::finished, this, [this, notifyIfUpToDate]() {
        QNetworkReply* reply = m_activeReply;
        m_activeReply = nullptr;
        reply->deleteLater();
        setChecking(false);

        if (reply->error() != QNetworkReply::NoError) {
            const QString error = QCoreApplication::translate("Core", "Update check failed: %1")
                                      .arg(reply->errorString());
            setLastError(error);
            setStatusText(error);
            emit updateFailed(error);
            return;
        }

        handleReleasePayload(reply->readAll(), notifyIfUpToDate);
    });
}

void AppUpdater::handleReleasePayload(const QByteArray& payload, bool notifyIfUpToDate)
{
    const QJsonObject release = QJsonDocument::fromJson(payload).object();
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
    m_downloadUrl.clear();

    const QString hint = preferredAssetNameHint();
    for (const QJsonValue& value : release.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        const QString url = asset.value(QStringLiteral("browser_download_url")).toString();
        if (name.isEmpty() || url.isEmpty())
            continue;
        if (!hint.isEmpty() && name.contains(hint, Qt::CaseInsensitive)) {
            m_downloadUrl = url;
            break;
        }
    }

    const bool available = compareVersions(currentVersion(), tag) < 0 && !m_downloadUrl.isEmpty();
    m_updateAvailable = available;

    if (available) {
        setStatusText(QCoreApplication::translate("Core", "Arachnel %1 is available").arg(tag));
    } else if (compareVersions(currentVersion(), tag) >= 0) {
        setStatusText(QCoreApplication::translate("Core", "Arachnel is up to date (%1)").arg(currentVersion()));
        if (!notifyIfUpToDate)
            setStatusText(QCoreApplication::translate("Core", "Arachnel is up to date (%1)").arg(currentVersion()));
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
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }

    m_activeReply = m_network->get(request);
    connect(m_activeReply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                m_downloadBytesTotal = total;
                if (total > 0)
                    m_downloadProgress = static_cast<int>((received * 100) / total);
                else
                    m_downloadProgress = 0;
                emit downloadProgressChanged();
            });
    connect(m_activeReply, &QNetworkReply::readyRead, this, [this, outFile]() {
        if (!m_activeReply || !outFile)
            return;
        outFile->write(m_activeReply->readAll());
    });

    connect(m_activeReply, &QNetworkReply::finished, this, [this, outFile, targetPath]() {
        QNetworkReply* reply = m_activeReply;
        m_activeReply = nullptr;

        if (reply)
            outFile->write(reply->readAll());
        outFile->close();
        outFile->deleteLater();
        if (reply)
            reply->deleteLater();

        if (!reply || reply->error() != QNetworkReply::NoError) {
            QFile::remove(targetPath);
            setDownloading(false);
            const QString error = QCoreApplication::translate("Core", "Download failed: %1")
                                      .arg(reply ? reply->errorString()
                                                 : QCoreApplication::translate("Core", "Unknown error"));
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
        setStatusText(QCoreApplication::translate("Core", "Starting updater…"));

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

bool AppUpdater::launchInstaller(const QString& installerPath, QString* errorOut)
{
#if defined(Q_OS_WIN)
    if (!QProcess::startDetached(installerPath, {QStringLiteral("--update")},
                                 QFileInfo(installerPath).absolutePath())) {
        if (errorOut) {
            *errorOut = QCoreApplication::translate("Core",
                                                    "Could not start the Arachnel installer");
        }
        return false;
    }
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

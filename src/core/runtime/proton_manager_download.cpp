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

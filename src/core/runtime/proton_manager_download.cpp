#include "proton_manager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
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
#include <QSysInfo>
#include <QUrl>
#include <QVariant>

namespace arachnel::core {

namespace {

enum class GeLookupFailure {
    None,
    RateLimit,
    ApiFailed,
    NoArchive,
};

struct GeReleaseLookup {
    QString versionName;
    QString downloadUrl;
    GeLookupFailure failure = GeLookupFailure::ApiFailed;
};

struct HttpGetResult {
    int status = 0;
    QByteArray body;
    QByteArray rateRemaining;
    QUrl url;
    QUrl redirectTarget;
};

// GE-Proton releases ship both x86_64 and aarch64 archives. Pick the one
// matching the host CPU - downloading the foreign arch is silently broken.
QString geProtonArchSuffix()
{
    const QString arch = QSysInfo::currentCpuArchitecture();
    if (arch == QLatin1String("arm64") || arch == QLatin1String("aarch64"))
        return QStringLiteral("aarch64");
    return QStringLiteral("x86_64");
}

bool pickGeProtonAsset(const QJsonArray& assets, QString* urlOut, QString* nameOut)
{
    QString fallbackUrl;
    QString fallbackName;
    const QString arch = geProtonArchSuffix();
    for (const QJsonValue& value : assets) {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (!name.startsWith(QStringLiteral("GE-Proton"))
            || !name.endsWith(QStringLiteral(".tar.gz"), Qt::CaseInsensitive)
            || name.contains(QStringLiteral("sha512"), Qt::CaseInsensitive))
            continue;
        const QString url = asset.value(QStringLiteral("browser_download_url")).toString();
        if (url.isEmpty())
            continue;
        const QString versionName =
            name.left(name.size() - QStringLiteral(".tar.gz").size());
        if (name.contains(arch, Qt::CaseInsensitive)) {
            *urlOut = url;
            *nameOut = versionName;
            return true;
        }
        if (fallbackUrl.isEmpty()) {
            fallbackUrl = url;
            fallbackName = versionName;
        }
    }
    if (!fallbackUrl.isEmpty()) {
        *urlOut = fallbackUrl;
        *nameOut = fallbackName;
        return true;
    }
    return false;
}

bool isGeProtonTag(const QString& tag)
{
    static const QRegularExpression re(QStringLiteral("^GE-Proton[0-9A-Za-z._+-]+$"));
    return re.match(tag).hasMatch();
}

bool fillGeReleaseFromTag(const QString& tag, QString* versionNameOut, QString* downloadUrlOut)
{
    const QString trimmed = tag.trimmed();
    if (!isGeProtonTag(trimmed) || !versionNameOut || !downloadUrlOut)
        return false;

    const QString archiveName =
        trimmed + QLatin1Char('-') + geProtonArchSuffix() + QStringLiteral(".tar.gz");
    *versionNameOut = archiveName.left(archiveName.size() - QStringLiteral(".tar.gz").size());
    *downloadUrlOut = QStringLiteral(
                          "https://github.com/GloriousEggroll/proton-ge-custom/releases/download/%1/%2")
                          .arg(trimmed, archiveName);
    return true;
}

QString geProtonTagFromUrl(const QUrl& url)
{
    if (!url.isValid())
        return {};

    static const QRegularExpression re(QStringLiteral("/releases/tag/([^/?#]+)"));
    const QRegularExpressionMatch match = re.match(url.toString());
    if (!match.hasMatch())
        return {};
    return QUrl::fromPercentEncoding(match.captured(1).toUtf8());
}

QString geProtonTagFromHttp(const HttpGetResult& page)
{
    const QString fromRedirect = geProtonTagFromUrl(page.redirectTarget);
    if (!fromRedirect.isEmpty())
        return fromRedirect;
    return geProtonTagFromUrl(page.url);
}

bool isGitHubRateLimit(int httpStatus, const QByteArray& rateRemaining, const QJsonObject& payload)
{
    if (httpStatus == 429)
        return true;

    const QString message = payload.value(QStringLiteral("message")).toString();
    if (message.contains(QStringLiteral("rate limit"), Qt::CaseInsensitive))
        return true;

    const QString docs = payload.value(QStringLiteral("documentation_url")).toString();
    if (docs.contains(QStringLiteral("rate-limit"), Qt::CaseInsensitive))
        return true;

    return (httpStatus == 403 || httpStatus == 401) && rateRemaining == QByteArrayLiteral("0");
}

GeReleaseLookup interpretGeReleaseBody(const QByteArray& body, int httpStatus,
                                       const QByteArray& rateRemaining)
{
    GeReleaseLookup result;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    const bool parsed = parseError.error == QJsonParseError::NoError && doc.isObject();
    const QJsonObject release = parsed ? doc.object() : QJsonObject();
    const bool looksLikeRelease = parsed && (release.contains(QStringLiteral("tag_name"))
                                              || release.contains(QStringLiteral("assets")));

    if (httpStatus < 200 || httpStatus >= 300 || !looksLikeRelease) {
        result.failure = isGitHubRateLimit(httpStatus, rateRemaining, release)
                             ? GeLookupFailure::RateLimit
                             : GeLookupFailure::ApiFailed;
        return result;
    }

    if (pickGeProtonAsset(release.value(QStringLiteral("assets")).toArray(), &result.downloadUrl,
                          &result.versionName)) {
        result.failure = GeLookupFailure::None;
        return result;
    }

    if (fillGeReleaseFromTag(release.value(QStringLiteral("tag_name")).toString(),
                             &result.versionName, &result.downloadUrl)) {
        result.failure = GeLookupFailure::None;
        return result;
    }

    result.failure = GeLookupFailure::NoArchive;
    return result;
}

QString geLookupErrorText(GeLookupFailure failure)
{
    switch (failure) {
    case GeLookupFailure::RateLimit:
        return QCoreApplication::translate("Core",
                                            "GitHub API rate limit reached. Try again later.");
    case GeLookupFailure::NoArchive:
        return QCoreApplication::translate("Core",
                                            "No Proton-GE archive found in latest release");
    case GeLookupFailure::None:
    case GeLookupFailure::ApiFailed:
        break;
    }
    return QCoreApplication::translate("Core", "GitHub API request failed.");
}

QUrl geLatestApiUrl()
{
    return QUrl(QStringLiteral(
        "https://api.github.com/repos/GloriousEggroll/proton-ge-custom/releases/latest"));
}

QUrl geLatestPageUrl()
{
    return QUrl(QStringLiteral(
        "https://github.com/GloriousEggroll/proton-ge-custom/releases/latest"));
}

void applyArachnelUserAgent(QNetworkRequest* request)
{
    request->setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel"));
}

void applyOptionalGitHubToken(QNetworkRequest* request)
{
    QString token = qEnvironmentVariable("GITHUB_TOKEN").trimmed();
    if (token.isEmpty())
        token = qEnvironmentVariable("GH_TOKEN").trimmed();
    if (token.isEmpty())
        return;
    request->setRawHeader("Authorization", ("Bearer " + token).toUtf8());
}

QNetworkRequest geApiRequest()
{
    QNetworkRequest request{geLatestApiUrl()};
    applyArachnelUserAgent(&request);
    applyOptionalGitHubToken(&request);
    return request;
}

QNetworkRequest geLatestPageRequest()
{
    QNetworkRequest request{geLatestPageUrl()};
    applyArachnelUserAgent(&request);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QVariant::fromValue(QNetworkRequest::ManualRedirectPolicy));
    return request;
}

QUrl redirectTargetFromReply(QNetworkReply* reply)
{
    QUrl target = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (!target.isValid()) {
        const QByteArray location = reply->rawHeader("Location");
        if (!location.isEmpty())
            target = QUrl::fromEncoded(location);
    }
    if (!target.isValid())
        return {};
    if (target.isRelative())
        target = reply->url().resolved(target);
    return target;
}

HttpGetResult httpResultFromReply(QNetworkReply* reply)
{
    HttpGetResult result;
    result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.body = reply->readAll();
    result.rateRemaining = reply->rawHeader("X-RateLimit-Remaining");
    result.url = reply->url();
    result.redirectTarget = redirectTargetFromReply(reply);
    return result;
}

HttpGetResult blockingGet(QNetworkAccessManager* network, const QNetworkRequest& request)
{
    QEventLoop loop;
    QNetworkReply* reply = network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const HttpGetResult result = httpResultFromReply(reply);
    reply->deleteLater();
    return result;
}

GeReleaseLookup lookupFromLatestPage(const HttpGetResult& page)
{
    GeReleaseLookup result;
    if (fillGeReleaseFromTag(geProtonTagFromHttp(page), &result.versionName, &result.downloadUrl))
        result.failure = GeLookupFailure::None;
    return result;
}

} // namespace

#include "proton_manager_helpers.h"

void ProtonManager::adoptLatestGeReleaseName(const QString& versionName)
{
    if (versionName.isEmpty() || m_latestGeReleaseName == versionName)
        return;
    m_latestGeReleaseName = versionName;
    emit latestGeReleaseChanged();
}

bool ProtonManager::fetchLatestGeReleaseInfo(QString* versionNameOut, QString* downloadUrlOut,
                                              QString* errorOut)
{
    if (!versionNameOut || !downloadUrlOut)
        return false;

    auto* network = new QNetworkAccessManager(this);
    const HttpGetResult api = blockingGet(network, geApiRequest());
    GeReleaseLookup lookup = interpretGeReleaseBody(api.body, api.status, api.rateRemaining);

    // 403 rate-limit bodies have no assets. The releases page redirect still has the tag.
    if (lookup.failure == GeLookupFailure::RateLimit
        || lookup.failure == GeLookupFailure::ApiFailed) {
        const GeReleaseLookup fromPage =
            lookupFromLatestPage(blockingGet(network, geLatestPageRequest()));
        if (fromPage.failure == GeLookupFailure::None)
            lookup = fromPage;
    }

    network->deleteLater();
    if (lookup.failure == GeLookupFailure::None) {
        *versionNameOut = lookup.versionName;
        *downloadUrlOut = lookup.downloadUrl;
        return true;
    }
    if (errorOut)
        *errorOut = geLookupErrorText(lookup.failure);
    return false;
}

void ProtonManager::refreshLatestGeRelease()
{
#if !defined(Q_OS_LINUX)
    return;
#else
    auto* network = new QNetworkAccessManager(this);
    QNetworkReply* reply = network->get(geApiRequest());
    connect(reply, &QNetworkReply::finished, this, [this, network, reply]() {
        const HttpGetResult api = httpResultFromReply(reply);
        reply->deleteLater();

        const GeReleaseLookup lookup =
            interpretGeReleaseBody(api.body, api.status, api.rateRemaining);
        if (lookup.failure == GeLookupFailure::None) {
            network->deleteLater();
            adoptLatestGeReleaseName(lookup.versionName);
            return;
        }
        if (lookup.failure == GeLookupFailure::NoArchive) {
            network->deleteLater();
            return;
        }

        QNetworkReply* pageReply = network->get(geLatestPageRequest());
        connect(pageReply, &QNetworkReply::finished, this, [this, network, pageReply]() {
            const GeReleaseLookup fromPage = lookupFromLatestPage(httpResultFromReply(pageReply));
            pageReply->deleteLater();
            network->deleteLater();
            if (fromPage.failure == GeLookupFailure::None)
                adoptLatestGeReleaseName(fromPage.versionName);
        });
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
    QString lookupError;
    if (!fetchLatestGeReleaseInfo(&versionName, &downloadUrl, &lookupError)) {
        if (lookupError.isEmpty())
            lookupError = QCoreApplication::translate("Core", "GitHub API request failed.");
        finishDownload(false, lookupError);
        return;
    }

    adoptLatestGeReleaseName(versionName);

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

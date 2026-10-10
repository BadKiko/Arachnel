#include "trailer_cache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>

namespace arachnel::core {

namespace {
constexpr qint64 kMaxTrailerBytes = 40LL * 1024 * 1024;   // a store "highlight" is a few MB
constexpr qint64 kCacheBudgetBytes = 256LL * 1024 * 1024;

bool isHttpUrl(const QString& url)
{
    return url.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || url.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
}
} // namespace

TrailerCache::TrailerCache(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

QString TrailerCache::cacheDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/trailer-cache");
}

QString TrailerCache::filePathFor(const QString& remoteUrl) const
{
    const QByteArray hash =
        QCryptographicHash::hash(remoteUrl.toUtf8(), QCryptographicHash::Sha1).toHex();
    const bool webm = QUrl(remoteUrl).path().endsWith(QStringLiteral(".webm"), Qt::CaseInsensitive);
    return cacheDir() + QLatin1Char('/') + QString::fromLatin1(hash)
        + (webm ? QStringLiteral(".webm") : QStringLiteral(".mp4"));
}

QString TrailerCache::localUrlFor(const QString& remoteUrl) const
{
    if (!isHttpUrl(remoteUrl))
        return {};
    const QString path = filePathFor(remoteUrl);
    return QFileInfo(path).size() > 0 ? QUrl::fromLocalFile(path).toString() : QString();
}

void TrailerCache::prefetch(const QString& remoteUrl)
{
    if (!isHttpUrl(remoteUrl) || m_inFlight.contains(remoteUrl) || !localUrlFor(remoteUrl).isEmpty())
        return;

    m_inFlight.insert(remoteUrl);
    QNetworkRequest request{QUrl(remoteUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network->get(request);
    reply->setProperty("remoteUrl", remoteUrl);
    // Refuse anything unexpectedly large before it fills memory and disk.
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64 total) {
        if (received > kMaxTrailerBytes || total > kMaxTrailerBytes)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleFinished(reply); });
}

void TrailerCache::handleFinished(QNetworkReply* reply)
{
    const QString remoteUrl = reply->property("remoteUrl").toString();
    m_inFlight.remove(remoteUrl);
    const bool ok = reply->error() == QNetworkReply::NoError
        && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() < 400;
    const QByteArray payload = ok ? reply->readAll() : QByteArray();
    reply->deleteLater();
    if (payload.isEmpty())
        return;

    QDir().mkpath(cacheDir());
    const QString path = filePathFor(remoteUrl);
    // Write beside, then rename: a half-written file must never look like a finished clip.
    const QString tmp = path + QStringLiteral(".part");
    QFile file(tmp);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    const bool written = file.write(payload) == payload.size();
    file.close();
    if (!written) {
        QFile::remove(tmp);
        return;
    }
    QFile::remove(path);
    if (!QFile::rename(tmp, path)) {
        QFile::remove(tmp);
        return;
    }
    trimToBudget();
    emit ready(remoteUrl, QUrl::fromLocalFile(path).toString());
}

void TrailerCache::trimToBudget()
{
    const QFileInfoList files = QDir(cacheDir()).entryInfoList(QDir::Files, QDir::Time);
    qint64 total = 0;
    for (const QFileInfo& f : files)
        total += f.size();
    // Newest first: drop from the old end until it fits (the newest clip always stays).
    for (int i = static_cast<int>(files.size()) - 1; i > 0 && total > kCacheBudgetBytes; --i) {
        total -= files.at(i).size();
        QFile::remove(files.at(i).absoluteFilePath());
    }
}

} // namespace arachnel::core

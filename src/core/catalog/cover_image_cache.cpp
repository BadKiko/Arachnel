#include "cover_image_cache.h"

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

bool isRemoteHttpUrl(const QString& url)
{
    return url.startsWith(QStringLiteral("http://"))
        || url.startsWith(QStringLiteral("https://"));
}

} // namespace

CoverImageCache::CoverImageCache(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    QDir().mkpath(cacheDir());
}

QString CoverImageCache::cacheDir() const
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/cover-cache");
    return dir;
}

QString CoverImageCache::extensionFor(const QString& remoteUrl)
{
    const QString path = QUrl(remoteUrl).path().toLower();
    if (path.endsWith(QStringLiteral(".png")))
        return QStringLiteral(".png");
    if (path.endsWith(QStringLiteral(".webp")))
        return QStringLiteral(".webp");
    if (path.endsWith(QStringLiteral(".jpg")) || path.endsWith(QStringLiteral(".jpeg")))
        return QStringLiteral(".jpg");
    return QStringLiteral(".jpg");
}

QString CoverImageCache::filePathFor(const QString& remoteUrl) const
{
    const QByteArray hash =
        QCryptographicHash::hash(remoteUrl.toUtf8(), QCryptographicHash::Sha1).toHex();
    return cacheDir() + QLatin1Char('/') + QString::fromLatin1(hash) + extensionFor(remoteUrl);
}

QString CoverImageCache::localUrlFor(const QString& remoteUrl) const
{
    if (remoteUrl.isEmpty())
        return {};
    if (remoteUrl.startsWith(QStringLiteral("file:")))
        return remoteUrl;
    if (!isRemoteHttpUrl(remoteUrl))
        return {};

    const QString path = filePathFor(remoteUrl);
    if (!QFileInfo::exists(path) || QFileInfo(path).size() <= 0)
        return {};
    return QUrl::fromLocalFile(path).toString();
}

bool CoverImageCache::has(const QString& remoteUrl) const
{
    return !localUrlFor(remoteUrl).isEmpty();
}

void CoverImageCache::ensure(const QString& remoteUrl)
{
    if (!isRemoteHttpUrl(remoteUrl))
        return;

    const QString local = localUrlFor(remoteUrl);
    if (!local.isEmpty()) {
        QMetaObject::invokeMethod(
            this, [this, remoteUrl, local]() { emit ready(remoteUrl, local); },
            Qt::QueuedConnection);
        return;
    }

    if (m_inFlight.contains(remoteUrl) || m_pending.contains(remoteUrl))
        return;

    m_pending.append(remoteUrl);
    startNext();
}

void CoverImageCache::remove(const QString& remoteUrl)
{
    if (remoteUrl.isEmpty())
        return;

    QString path;
    if (remoteUrl.startsWith(QStringLiteral("file:")))
        path = QUrl(remoteUrl).toLocalFile();
    else if (isRemoteHttpUrl(remoteUrl))
        path = filePathFor(remoteUrl);

    if (!path.isEmpty())
        QFile::remove(path);

    m_pending.removeAll(remoteUrl);
}

void CoverImageCache::startNext()
{
    while (m_active < kMaxConcurrent && !m_pending.isEmpty()) {
        const QString remoteUrl = m_pending.takeFirst();
        if (m_inFlight.contains(remoteUrl))
            continue;

        const QString existing = localUrlFor(remoteUrl);
        if (!existing.isEmpty()) {
            emit ready(remoteUrl, existing);
            continue;
        }

        m_inFlight.insert(remoteUrl);
        ++m_active;

        QNetworkRequest request{QUrl(remoteUrl)};
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
        request.setTransferTimeout(30000);
        QNetworkReply* reply = m_network->get(request);
        reply->setProperty("remoteUrl", remoteUrl);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleFinished(reply); });
    }
}

void CoverImageCache::handleFinished(QNetworkReply* reply)
{
    const QString remoteUrl = reply->property("remoteUrl").toString();
    m_inFlight.remove(remoteUrl);
    --m_active;

    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        emit failed(remoteUrl);
        startNext();
        return;
    }

    const QByteArray payload = reply->readAll();
    reply->deleteLater();

    if (payload.isEmpty()) {
        emit failed(remoteUrl);
        startNext();
        return;
    }

    const QString path = filePathFor(remoteUrl);
    QDir().mkpath(cacheDir());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit failed(remoteUrl);
        startNext();
        return;
    }
    file.write(payload);
    file.close();

    emit ready(remoteUrl, QUrl::fromLocalFile(path).toString());
    startNext();
}

} // namespace arachnel::core

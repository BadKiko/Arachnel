#include "cover_image_cache.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>

namespace arachnel::core {

bool CoverImageCache::isRemoteHttpUrl(const QString& url)
{
    return url.startsWith(QStringLiteral("http://"))
        || url.startsWith(QStringLiteral("https://"));
}

CoverImageCache::CoverImageCache(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    QDir().mkpath(cacheDir());
    loadNegativeCache();
}

QString CoverImageCache::cacheDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/cover-cache");
}

QString CoverImageCache::failedCachePath() const
{
    return cacheDir() + QStringLiteral("/failed.tsv");
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

void CoverImageCache::loadNegativeCache()
{
    QFile file(failedCachePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const QStringList parts = line.split(QLatin1Char('\t'));
        if (parts.size() < 2)
            continue;
        const QString url = parts.at(0);
        const qint64 at = parts.at(1).toLongLong();
        if (url.isEmpty() || at <= 0)
            continue;
        if (now - at > kNegativeTtlMs)
            continue;
        m_failedAtMs.insert(url, at);
    }
}

void CoverImageCache::persistNegativeCache() const
{
    if (!m_negativeDirty)
        return;
    QDir().mkpath(cacheDir());
    QFile file(failedCachePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << QStringLiteral("# url\tfailedAtMs\n");
    for (auto it = m_failedAtMs.constBegin(); it != m_failedAtMs.constEnd(); ++it)
        out << it.key() << QLatin1Char('\t') << it.value() << QLatin1Char('\n');
}

QString CoverImageCache::localUrlFor(const QString& remoteUrl) const
{
    if (remoteUrl.isEmpty())
        return {};
    if (remoteUrl.startsWith(QStringLiteral("file:"))) {
        const QString path = QUrl(remoteUrl).toLocalFile();
        if (path.isEmpty() || !QFileInfo::exists(path) || QFileInfo(path).size() <= 0)
            return {};
        return remoteUrl;
    }
    if (!isRemoteHttpUrl(remoteUrl))
        return {};
    if (hasFailed(remoteUrl))
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

bool CoverImageCache::hasFailed(const QString& remoteUrl) const
{
    const auto it = m_failedAtMs.constFind(remoteUrl);
    if (it == m_failedAtMs.cend())
        return false;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    return (now - it.value()) <= kNegativeTtlMs;
}

void CoverImageCache::markFailed(const QString& remoteUrl)
{
    m_failedAtMs.insert(remoteUrl, QDateTime::currentMSecsSinceEpoch());
    m_negativeDirty = true;
    persistNegativeCache();
}

void CoverImageCache::clearFailed(const QString& remoteUrl)
{
    if (!m_failedAtMs.remove(remoteUrl))
        return;
    m_negativeDirty = true;
    persistNegativeCache();
}

void CoverImageCache::enqueue(const QString& remoteUrl, CoverFetchPriority priority)
{
    const auto existing = m_pendingPriority.constFind(remoteUrl);
    if (existing != m_pendingPriority.cend()) {
        if (static_cast<int>(priority) <= static_cast<int>(existing.value()))
            return;
        for (int i = 0; i < m_pending.size(); ++i) {
            if (m_pending.at(i).url == remoteUrl) {
                m_pending.removeAt(i);
                break;
            }
        }
    }

    m_pendingPriority.insert(remoteUrl, priority);

    // Higher priority starts sooner (Visible > Warm > Upgrade).
    int insertAt = m_pending.size();
    for (int i = 0; i < m_pending.size(); ++i) {
        if (static_cast<int>(m_pending.at(i).priority) < static_cast<int>(priority)) {
            insertAt = i;
            break;
        }
    }
    m_pending.insert(insertAt, PendingItem{remoteUrl, priority});
}

void CoverImageCache::ensure(const QString& remoteUrl, CoverFetchPriority priority)
{
    if (!isRemoteHttpUrl(remoteUrl))
        return;

    if (hasFailed(remoteUrl)) {
        QMetaObject::invokeMethod(
            this, [this, remoteUrl]() { emit failed(remoteUrl); }, Qt::QueuedConnection);
        return;
    }

    const QString local = localUrlFor(remoteUrl);
    if (!local.isEmpty()) {
        QMetaObject::invokeMethod(
            this, [this, remoteUrl, local]() { emit ready(remoteUrl, local); },
            Qt::QueuedConnection);
        return;
    }

    if (m_inFlight.contains(remoteUrl)) {
        // Already downloading — bump is unnecessary; waiter lives in coordinator.
        return;
    }

    enqueue(remoteUrl, priority);
    startNext();
}

void CoverImageCache::remove(const QString& remoteUrl)
{
    if (remoteUrl.isEmpty())
        return;

    clearFailed(remoteUrl);

    QString path;
    if (remoteUrl.startsWith(QStringLiteral("file:")))
        path = QUrl(remoteUrl).toLocalFile();
    else if (isRemoteHttpUrl(remoteUrl))
        path = filePathFor(remoteUrl);

    if (!path.isEmpty())
        QFile::remove(path);

    m_pendingPriority.remove(remoteUrl);
    for (int i = m_pending.size() - 1; i >= 0; --i) {
        if (m_pending.at(i).url == remoteUrl)
            m_pending.removeAt(i);
    }

    if (m_inFlight.contains(remoteUrl)) {
        m_ignoreWhenFinished.insert(remoteUrl);
        if (QNetworkReply* reply = m_replies.value(remoteUrl).data())
            reply->abort();
    }
}

void CoverImageCache::startNext()
{
    while (m_active < kMaxConcurrent && !m_pending.isEmpty()) {
        // Highest priority first (Visible at front after enqueue policy).
        int bestIdx = 0;
        for (int i = 1; i < m_pending.size(); ++i) {
            if (static_cast<int>(m_pending.at(i).priority)
                > static_cast<int>(m_pending.at(bestIdx).priority))
                bestIdx = i;
        }
        const PendingItem item = m_pending.takeAt(bestIdx);
        m_pendingPriority.remove(item.url);

        if (m_inFlight.contains(item.url))
            continue;

        if (hasFailed(item.url)) {
            emit failed(item.url);
            continue;
        }

        const QString existing = localUrlFor(item.url);
        if (!existing.isEmpty()) {
            emit ready(item.url, existing);
            continue;
        }

        m_inFlight.insert(item.url);
        ++m_active;

        QNetworkRequest request{QUrl(item.url)};
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
        request.setTransferTimeout(20000);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* reply = m_network->get(request);
        reply->setProperty("remoteUrl", item.url);
        m_replies.insert(item.url, reply);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleFinished(reply); });
    }
}

void CoverImageCache::handleFinished(QNetworkReply* reply)
{
    const QString remoteUrl = reply->property("remoteUrl").toString();
    m_inFlight.remove(remoteUrl);
    m_replies.remove(remoteUrl);
    --m_active;

    const bool ignore = m_ignoreWhenFinished.remove(remoteUrl);
    const auto error = reply->error();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload =
        (error == QNetworkReply::NoError && status < 400) ? reply->readAll() : QByteArray{};
    reply->deleteLater();

    if (ignore) {
        startNext();
        return;
    }

    if (error != QNetworkReply::NoError || status >= 400 || payload.isEmpty()) {
        markFailed(remoteUrl);
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

    clearFailed(remoteUrl);
    emit ready(remoteUrl, QUrl::fromLocalFile(path).toString());
    startNext();
}

} // namespace arachnel::core

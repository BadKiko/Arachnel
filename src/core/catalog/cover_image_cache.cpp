#include "cover_image_cache.h"

#include <algorithm>

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
#include <QVariantMap>

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
    m_persistTimer.setSingleShot(true);
    m_persistTimer.setInterval(1000);
    connect(&m_persistTimer, &QTimer::timeout, this, [this]() { persistNegativeCache(); });
    m_recentLatenciesMs.resize(kLatencyWindow);
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
    m_negativeDirty = false;
}

void CoverImageCache::schedulePersistNegativeCache()
{
    m_negativeDirty = true;
    if (!m_persistTimer.isActive())
        m_persistTimer.start();
}

void CoverImageCache::rememberPositive(const QString& remoteUrl, const QString& localUrl) const
{
    if (remoteUrl.isEmpty() || localUrl.isEmpty())
        return;
    // Soft cap on the positive URL map.
    constexpr int kMaxPositive = 8192;
    if (m_positiveLocal.size() >= kMaxPositive)
        m_positiveLocal.clear();
    m_positiveLocal.insert(remoteUrl, localUrl);
}

QString CoverImageCache::localUrlFor(const QString& remoteUrl) const
{
    if (remoteUrl.isEmpty())
        return {};
    if (remoteUrl.startsWith(QStringLiteral("file:"))) {
        const auto cached = m_positiveLocal.constFind(remoteUrl);
        if (cached != m_positiveLocal.cend())
            return cached.value();
        const QString path = QUrl(remoteUrl).toLocalFile();
        if (path.isEmpty() || !QFileInfo::exists(path) || QFileInfo(path).size() <= 0)
            return {};
        rememberPositive(remoteUrl, remoteUrl);
        return remoteUrl;
    }
    if (!isRemoteHttpUrl(remoteUrl))
        return {};
    if (hasFailed(remoteUrl))
        return {};

    const auto cached = m_positiveLocal.constFind(remoteUrl);
    if (cached != m_positiveLocal.cend())
        return cached.value();

    const QString path = filePathFor(remoteUrl);
    if (!QFileInfo::exists(path) || QFileInfo(path).size() <= 0)
        return {};
    const QString local = QUrl::fromLocalFile(path).toString();
    rememberPositive(remoteUrl, local);
    return local;
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
    m_positiveLocal.remove(remoteUrl);
    schedulePersistNegativeCache();
}

void CoverImageCache::clearFailed(const QString& remoteUrl)
{
    if (!m_failedAtMs.remove(remoteUrl))
        return;
    schedulePersistNegativeCache();
}

void CoverImageCache::clearAllFailed()
{
    if (m_failedAtMs.isEmpty())
        return;
    m_failedAtMs.clear();
    schedulePersistNegativeCache();
}

void CoverImageCache::noteCacheHit()
{
    ++m_cacheHits;
}

void CoverImageCache::removePending(const QString& remoteUrl)
{
    m_pendingPriority.remove(remoteUrl);
    for (int i = m_pending.size() - 1; i >= 0; --i) {
        if (m_pending.at(i).url == remoteUrl)
            m_pending.removeAt(i);
    }
}

void CoverImageCache::trimPending()
{
    while (m_pending.size() > kMaxPending) {
        int dropIdx = -1;
        for (int i = m_pending.size() - 1; i >= 0; --i) {
            if (m_pending.at(i).priority != CoverFetchPriority::Visible) {
                dropIdx = i;
                break;
            }
        }
        // Never drop Visible - jump-scroll can briefly queue a full viewport.
        if (dropIdx < 0)
            break;
        const QString url = m_pending.takeAt(dropIdx).url;
        m_pendingPriority.remove(url);
    }
}

void CoverImageCache::enqueue(const QString& remoteUrl, CoverFetchPriority priority)
{
    removePending(remoteUrl);
    m_pendingPriority.insert(remoteUrl, priority);

    int insertAt = 0;
    if (priority == CoverFetchPriority::Visible) {
        // Newest Visible always jumps the queue (what the user is looking at now).
        insertAt = 0;
    } else {
        insertAt = m_pending.size();
        for (int i = 0; i < m_pending.size(); ++i) {
            if (static_cast<int>(m_pending.at(i).priority) < static_cast<int>(priority)) {
                insertAt = i;
                break;
            }
        }
    }
    m_pending.insert(insertAt, PendingItem{remoteUrl, priority});
    trimPending();
}

void CoverImageCache::release(const QString& remoteUrl)
{
    if (remoteUrl.isEmpty())
        return;
    removePending(remoteUrl);
    if (m_inFlight.contains(remoteUrl)) {
        // Still downloading for a scrolled-away card - demote so Visible can steal the slot.
        m_inFlightPriority.insert(remoteUrl, CoverFetchPriority::Upgrade);
        preemptForVisible();
    }
    startNext();
}

bool CoverImageCache::isInFlight(const QString& remoteUrl) const
{
    return !remoteUrl.isEmpty() && m_inFlight.contains(remoteUrl);
}

void CoverImageCache::ensure(const QString& remoteUrl, CoverFetchPriority priority)
{
    if (!isRemoteHttpUrl(remoteUrl))
        return;

    ++m_ensureCalls;

    if (hasFailed(remoteUrl)) {
        ++m_negativeHits;
        QMetaObject::invokeMethod(
            this, [this, remoteUrl]() { emit failed(remoteUrl); }, Qt::QueuedConnection);
        return;
    }

    const QString local = localUrlFor(remoteUrl);
    if (!local.isEmpty()) {
        ++m_cacheHits;
        QMetaObject::invokeMethod(
            this, [this, remoteUrl, local]() { emit ready(remoteUrl, local); },
            Qt::QueuedConnection);
        return;
    }

    if (m_inFlight.contains(remoteUrl)) {
        if (static_cast<int>(priority)
            > static_cast<int>(m_inFlightPriority.value(remoteUrl, CoverFetchPriority::Upgrade)))
            m_inFlightPriority.insert(remoteUrl, priority);
        return;
    }

    enqueue(remoteUrl, priority);
    if (priority == CoverFetchPriority::Visible)
        preemptForVisible();
    startNext();
}

void CoverImageCache::noteLatency(qint64 ms)
{
    if (ms < 0)
        ms = 0;
    m_latencySumMs += ms;
    if (ms > m_latencyMaxMs)
        m_latencyMaxMs = ms;
    m_recentLatenciesMs[m_recentWrite % kLatencyWindow] = static_cast<int>(ms);
    ++m_recentWrite;
    refreshPercentiles();
}

void CoverImageCache::refreshPercentiles()
{
    const int n = qMin(m_recentWrite, kLatencyWindow);
    if (n <= 0) {
        m_p50Ms = 0;
        m_p95Ms = 0;
        return;
    }
    QVector<int> sorted;
    sorted.reserve(n);
    const int start = m_recentWrite >= kLatencyWindow ? (m_recentWrite % kLatencyWindow) : 0;
    for (int i = 0; i < n; ++i) {
        const int idx = m_recentWrite >= kLatencyWindow ? (start + i) % kLatencyWindow : i;
        sorted.append(m_recentLatenciesMs.at(idx));
    }
    std::sort(sorted.begin(), sorted.end());
    m_p50Ms = sorted.at(qMax(0, (n - 1) / 2));
    m_p95Ms = sorted.at(qMax(0, (n * 95) / 100 - (n >= 20 ? 1 : 0)));
    if (m_p95Ms < m_p50Ms)
        m_p95Ms = sorted.last();
}

CoverCacheStats CoverImageCache::stats() const
{
    CoverCacheStats s;
    s.ensureCalls = m_ensureCalls;
    s.cacheHits = m_cacheHits;
    s.negativeHits = m_negativeHits;
    s.downloadsStarted = m_downloadsStarted;
    s.downloadsOk = m_downloadsOk;
    s.downloadsFail = m_downloadsFail;
    s.preempts = m_preempts;
    s.latencySumMs = m_latencySumMs;
    s.latencyMaxMs = m_latencyMaxMs;
    s.pending = m_pending.size();
    s.active = m_active;
    s.p50Ms = m_p50Ms;
    s.p95Ms = m_p95Ms;
    return s;
}

QVariantMap CoverImageCache::statsMap() const
{
    const CoverCacheStats s = stats();
    return QVariantMap{
        {QStringLiteral("ensureCalls"), s.ensureCalls},
        {QStringLiteral("cacheHits"), s.cacheHits},
        {QStringLiteral("negativeHits"), s.negativeHits},
        {QStringLiteral("downloadsStarted"), s.downloadsStarted},
        {QStringLiteral("downloadsOk"), s.downloadsOk},
        {QStringLiteral("downloadsFail"), s.downloadsFail},
        {QStringLiteral("preempts"), s.preempts},
        {QStringLiteral("latencyAvgMs"),
         s.downloadsOk > 0 ? static_cast<qint64>(s.latencySumMs / s.downloadsOk) : 0},
        {QStringLiteral("latencyMaxMs"), s.latencyMaxMs},
        {QStringLiteral("p50Ms"), s.p50Ms},
        {QStringLiteral("p95Ms"), s.p95Ms},
        {QStringLiteral("pending"), s.pending},
        {QStringLiteral("active"), s.active},
    };
}

void CoverImageCache::resetStats()
{
    m_ensureCalls = 0;
    m_cacheHits = 0;
    m_negativeHits = 0;
    m_downloadsStarted = 0;
    m_downloadsOk = 0;
    m_downloadsFail = 0;
    m_preempts = 0;
    m_latencySumMs = 0;
    m_latencyMaxMs = 0;
    m_p50Ms = 0;
    m_p95Ms = 0;
    m_recentWrite = 0;
    m_recentLatenciesMs.fill(0);
}

void CoverImageCache::preemptForVisible()
{
    if (m_active < kMaxConcurrent)
        return;

    int visiblePending = 0;
    for (const PendingItem& item : m_pending) {
        if (item.priority == CoverFetchPriority::Visible)
            ++visiblePending;
    }
    if (visiblePending <= 0)
        return;

    int inFlightVisible = 0;
    for (auto it = m_inFlightPriority.constBegin(); it != m_inFlightPriority.constEnd(); ++it) {
        if (it.value() == CoverFetchPriority::Visible)
            ++inFlightVisible;
    }

    int toFree = qMin(visiblePending, kMaxConcurrent - inFlightVisible);
    while (toFree > 0) {
        QString victim;
        CoverFetchPriority victimPri = CoverFetchPriority::Visible;
        for (auto it = m_inFlightPriority.constBegin(); it != m_inFlightPriority.constEnd(); ++it) {
            if (m_preempted.contains(it.key()))
                continue;
            if (static_cast<int>(it.value()) >= static_cast<int>(CoverFetchPriority::Visible))
                continue;
            if (victim.isEmpty() || static_cast<int>(it.value()) < static_cast<int>(victimPri)) {
                victim = it.key();
                victimPri = it.value();
            }
        }
        if (victim.isEmpty())
            break;

        QNetworkReply* reply = m_replies.value(victim).data();
        if (!reply)
            break;
        m_preempted.insert(victim);
        ++m_preempts;
        reply->abort();
        --toFree;
    }
}

void CoverImageCache::remove(const QString& remoteUrl)
{
    if (remoteUrl.isEmpty())
        return;

    clearFailed(remoteUrl);
    m_positiveLocal.remove(remoteUrl);

    QString path;
    if (remoteUrl.startsWith(QStringLiteral("file:")))
        path = QUrl(remoteUrl).toLocalFile();
    else if (isRemoteHttpUrl(remoteUrl))
        path = filePathFor(remoteUrl);

    if (!path.isEmpty())
        QFile::remove(path);

    removePending(remoteUrl);

    if (m_inFlight.contains(remoteUrl)) {
        m_ignoreWhenFinished.insert(remoteUrl);
        m_preempted.remove(remoteUrl);
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
        m_inFlightPriority.insert(item.url, item.priority);
        m_downloadStartedAtMs.insert(item.url, QDateTime::currentMSecsSinceEpoch());
        ++m_active;
        ++m_downloadsStarted;

        QNetworkRequest request{QUrl(item.url)};
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
        request.setTransferTimeout(15000);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
        QNetworkReply* reply = m_network->get(request);
        reply->setProperty("remoteUrl", item.url);
        m_replies.insert(item.url, reply);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleFinished(reply); });
    }
}

void CoverImageCache::handleFinished(QNetworkReply* reply)
{
    const QString remoteUrl = reply->property("remoteUrl").toString();
    const CoverFetchPriority finishedPri = m_inFlightPriority.take(remoteUrl);
    const qint64 startedAt = m_downloadStartedAtMs.take(remoteUrl);
    m_inFlight.remove(remoteUrl);
    m_replies.remove(remoteUrl);
    --m_active;

    const bool ignore = m_ignoreWhenFinished.remove(remoteUrl);
    const bool preempted = m_preempted.remove(remoteUrl);
    const auto error = reply->error();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload =
        (error == QNetworkReply::NoError && status < 400) ? reply->readAll() : QByteArray{};
    reply->deleteLater();

    if (ignore) {
        startNext();
        return;
    }

    if (preempted) {
        enqueue(remoteUrl, finishedPri);
        startNext();
        return;
    }

    if (error == QNetworkReply::OperationCanceledError) {
        startNext();
        return;
    }

    if (error != QNetworkReply::NoError || status >= 400 || payload.isEmpty()) {
        ++m_downloadsFail;
        markFailed(remoteUrl);
        emit failed(remoteUrl);
        startNext();
        return;
    }

    const QString path = filePathFor(remoteUrl);
    QDir().mkpath(cacheDir());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        ++m_downloadsFail;
        emit failed(remoteUrl);
        startNext();
        return;
    }
    file.write(payload);
    file.close();

    if (startedAt > 0)
        noteLatency(QDateTime::currentMSecsSinceEpoch() - startedAt);
    ++m_downloadsOk;

    clearFailed(remoteUrl);
    const QString local = QUrl::fromLocalFile(path).toString();
    rememberPositive(remoteUrl, local);
    emit ready(remoteUrl, local);
    startNext();
}

} // namespace arachnel::core

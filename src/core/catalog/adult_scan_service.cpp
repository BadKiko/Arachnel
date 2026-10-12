#include "adult_scan_service.h"

#include "catalog_disk_cache.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace arachnel::core {

namespace {

// The batch endpoint is a GET and Steam rejects URLs over ~8 KB: 200 ids stay well below it.
constexpr int kBatchSize = 200;
constexpr int kPumpDelayMs = 500;
constexpr int kMaxWanted = 3000;
constexpr qint64 kRetryAfterSecs = 120;
constexpr int kCacheVersion = 2;

// Content descriptors: 1 some nudity or sexual content, 3 adult only sexual content, 4 frequent
// nudity or sexual content.
bool isAdultDescriptor(int id)
{
    return id == 1 || id == 3 || id == 4;
}

QJsonArray toJson(const QSet<quint32>& ids)
{
    QJsonArray out;
    for (quint32 id : ids)
        out.append(static_cast<double>(id));
    return out;
}

QSet<quint32> fromJson(const QJsonArray& arr)
{
    QSet<quint32> out;
    out.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        const qint64 id = static_cast<qint64>(v.toDouble());
        if (id > 0 && id <= 0xFFFFFFFFLL)
            out.insert(static_cast<quint32>(id));
    }
    return out;
}

} // namespace

AdultScanService::AdultScanService(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_pumpTimer(new QTimer(this))
    , m_saveTimer(new QTimer(this))
    , m_signalTimer(new QTimer(this))
{
    m_pumpTimer->setSingleShot(true);
    connect(m_pumpTimer, &QTimer::timeout, this, &AdultScanService::pump);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(5000);
    connect(m_saveTimer, &QTimer::timeout, this, [this]() { save(); });
    // Several batches often land together; one signal means one refilter.
    m_signalTimer->setSingleShot(true);
    m_signalTimer->setInterval(700);
    connect(m_signalTimer, &QTimer::timeout, this, [this]() {
        if (!m_foundSinceSignal)
            return;
        m_foundSinceSignal = false;
        emit foundAdult();
    });
}

QString AdultScanService::cacheFilePath()
{
    return CatalogDiskCache::cacheDir() + QStringLiteral("/adult-scan.json");
}

void AdultScanService::loadCache()
{
    QFile file(cacheFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    if (obj.value(QStringLiteral("version")).toInt() != kCacheVersion)
        return;
    m_adult = fromJson(obj.value(QStringLiteral("adult")).toArray());
    m_checked = fromJson(obj.value(QStringLiteral("checked")).toArray());
    m_unlisted = fromJson(obj.value(QStringLiteral("unlisted")).toArray());
    m_checked.unite(m_adult);
}

bool AdultScanService::isUnlisted(const QString& steamAppId) const
{
    if (m_unlisted.isEmpty() || steamAppId.isEmpty())
        return false;
    bool ok = false;
    const uint id = steamAppId.toUInt(&ok);
    return ok && m_unlisted.contains(id);
}

bool AdultScanService::contains(const QString& steamAppId) const
{
    if (m_adult.isEmpty() || steamAppId.isEmpty())
        return false;
    bool ok = false;
    const uint id = steamAppId.toUInt(&ok);
    return ok && m_adult.contains(id);
}

void AdultScanService::setWanted(const QVector<quint32>& appIds)
{
    m_wanted.clear();
    m_wanted.reserve(qMin<int>(appIds.size(), kMaxWanted));
    for (quint32 id : appIds) {
        if (id == 0 || m_checked.contains(id))
            continue;
        m_wanted.append(id);
        if (m_wanted.size() >= kMaxWanted)
            break;
    }
    if (!m_wanted.isEmpty() && !m_running && !m_pumpTimer->isActive())
        m_pumpTimer->start(0);
}

void AdultScanService::cancel()
{
    m_wanted.clear();
}

void AdultScanService::pump()
{
    if (m_running || m_wanted.isEmpty())
        return;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now < m_retryNotBefore) {
        m_pumpTimer->start(static_cast<int>((m_retryNotBefore - now) * 1000));
        return;
    }

    QVector<quint32> batch;
    batch.reserve(kBatchSize);
    while (!m_wanted.isEmpty() && batch.size() < kBatchSize) {
        const quint32 id = m_wanted.takeFirst();
        if (!m_checked.contains(id))
            batch.append(id);
    }
    if (batch.isEmpty())
        return;

    QJsonArray ids;
    for (quint32 id : batch) {
        QJsonObject o;
        o.insert(QStringLiteral("appid"), static_cast<double>(id));
        ids.append(o);
    }
    QJsonObject context;
    context.insert(QStringLiteral("language"), QStringLiteral("english"));
    context.insert(QStringLiteral("country_code"), QStringLiteral("US"));
    QJsonObject input;
    input.insert(QStringLiteral("ids"), ids);
    input.insert(QStringLiteral("context"), context);
    input.insert(QStringLiteral("data_request"), QJsonObject());

    QUrl url(QStringLiteral("https://api.steampowered.com/IStoreBrowseService/GetItems/v1"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("input_json"),
                   QString::fromUtf8(QJsonDocument(input).toJson(QJsonDocument::Compact)));
    url.setQuery(q);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
    request.setTransferTimeout(30000);
    m_running = true;
    QNetworkReply* reply = m_network->get(request);
    reply->setProperty("batch", QVariant::fromValue(batch));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleReply(reply); });
}

void AdultScanService::handleReply(QNetworkReply* reply)
{
    reply->deleteLater();
    m_running = false;
    const QVector<quint32> batch = reply->property("batch").value<QVector<quint32>>();

    const bool transportOk = reply->error() == QNetworkReply::NoError;
    QJsonArray items;
    if (transportOk) {
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        items = root.value(QStringLiteral("response"))
                    .toObject()
                    .value(QStringLiteral("store_items"))
                    .toArray();
    }
    if (!transportOk || items.isEmpty()) {
        // Offline, rate limited or an odd answer: nothing is marked, the batch is asked again.
        m_retryNotBefore = QDateTime::currentSecsSinceEpoch() + kRetryAfterSecs;
        QVector<quint32> again = batch;
        again.append(m_wanted);
        m_wanted = again;
        if (!m_pumpTimer->isActive())
            m_pumpTimer->start(kRetryAfterSecs * 1000);
        return;
    }

    for (quint32 id : batch)
        m_checked.insert(id);
    bool newAdult = false;
    bool newUnlisted = false;
    const auto markAdult = [&](quint32 appId) {
        if (appId == 0 || m_adult.contains(appId))
            return;
        m_adult.insert(appId);
        m_checked.insert(appId);
        newAdult = true;
    };
    QVector<quint32> parentsToAsk;
    QVector<quint32> childrenToRetry;
    for (const QJsonValue& value : items) {
        const QJsonObject item = value.toObject();
        if (item.value(QStringLiteral("success")).toInt(1) != 1) {
            // Not listed on the store ("id" is the app that was asked about).
            const quint32 missing = static_cast<quint32>(item.value(QStringLiteral("id")).toDouble());
            if (missing != 0 && !m_unlisted.contains(missing)) {
                m_unlisted.insert(missing);
                newUnlisted = true;
            }
            continue;
        }
        const quint32 appId = static_cast<quint32>(item.value(QStringLiteral("appid")).toDouble());
        if (appId == 0)
            continue;
        const QJsonObject related = item.value(QStringLiteral("related_items")).toObject();
        bool adult = false;
        for (const QJsonValue& d : item.value(QStringLiteral("content_descriptorids")).toArray()) {
            if (isAdultDescriptor(d.toInt())) {
                adult = true;
                break;
            }
        }
        if (adult) {
            markAdult(appId);
            // A demo store page carries no descriptors of its own (and often is not even served);
            // it is as adult as its game.
            for (const QJsonValue& demo : related.value(QStringLiteral("demos")).toArray())
                markAdult(static_cast<quint32>(demo.toObject().value(QStringLiteral("appid")).toDouble()));
            continue;
        }
        // Soundtracks, DLC and tools say nothing either: they follow their parent game.
        const quint32 parent = static_cast<quint32>(related.value(QStringLiteral("parent_appid")).toDouble());
        if (parent == 0)
            continue;
        if (m_adult.contains(parent)) {
            markAdult(appId);
        } else if (!m_checked.contains(parent) && !m_parentAsked.contains(appId)) {
            m_parentAsked.insert(appId);
            m_checked.remove(appId);
            parentsToAsk.append(parent);
            childrenToRetry.append(appId);
        }
    }
    if (!parentsToAsk.isEmpty()) {
        // Parents first, then the children again, ahead of the rest of the wishlist.
        QVector<quint32> next = parentsToAsk;
        next.append(childrenToRetry);
        next.append(m_wanted);
        m_wanted = next;
    }
    if (newAdult || newUnlisted) {
        m_foundSinceSignal = true;
        m_signalTimer->start();
    }
    scheduleSave();
    if (!m_wanted.isEmpty())
        m_pumpTimer->start(kPumpDelayMs);
}

void AdultScanService::scheduleSave()
{
    if (!m_saveTimer->isActive())
        m_saveTimer->start();
}

void AdultScanService::save() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("version"), kCacheVersion);
    obj.insert(QStringLiteral("adult"), toJson(m_adult));
    obj.insert(QStringLiteral("checked"), toJson(m_checked));
    obj.insert(QStringLiteral("unlisted"), toJson(m_unlisted));
    QSaveFile file(cacheFilePath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    file.commit();
}

} // namespace arachnel::core

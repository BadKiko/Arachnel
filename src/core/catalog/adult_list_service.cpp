#include "adult_list_service.h"

#include "catalog_disk_cache.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QUrl>

namespace arachnel::core {

namespace {

// Same host as the Steam catalog; the file format is {"appIds":[...]} (scripts/adult/scan_adult.py).
constexpr const char* kAdultListUrl = "https://ryuu.badkiko.ru/v1/adult-ids";

} // namespace

AdultListService::AdultListService(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

QString AdultListService::cacheFilePath()
{
    return CatalogDiskCache::cacheDir() + QStringLiteral("/adult-ids.json");
}

void AdultListService::loadCache()
{
    QFile file(cacheFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    QSet<quint32> ids;
    const QJsonArray arr = obj.value(QStringLiteral("appIds")).toArray();
    ids.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        const qint64 id = static_cast<qint64>(v.toDouble());
        if (id > 0 && id <= 0xFFFFFFFFLL)
            ids.insert(static_cast<quint32>(id));
    }
    m_ids = std::move(ids);
    m_etag = obj.value(QStringLiteral("etag")).toString().toUtf8();
}

bool AdultListService::contains(const QString& steamAppId) const
{
    if (m_ids.isEmpty() || steamAppId.isEmpty())
        return false;
    bool ok = false;
    const uint id = steamAppId.toUInt(&ok);
    return ok && m_ids.contains(id);
}

void AdultListService::refresh()
{
    if (m_running)
        return;
    QNetworkRequest request{QUrl(QString::fromLatin1(kAdultListUrl))};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
    request.setTransferTimeout(20000);
    if (!m_etag.isEmpty() && !m_ids.isEmpty())
        request.setRawHeader("If-None-Match", m_etag);
    m_running = true;
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleReply(reply); });
}

void AdultListService::handleReply(QNetworkReply* reply)
{
    reply->deleteLater();
    m_running = false;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || status != 200)
        return; // offline, 304 (unchanged) or no list on this server: keep what is cached

    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    const QJsonArray arr = obj.value(QStringLiteral("appIds")).toArray();
    if (arr.isEmpty())
        return; // never replace a list with an empty or broken one
    QSet<quint32> ids;
    ids.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        const qint64 id = static_cast<qint64>(v.toDouble());
        if (id > 0 && id <= 0xFFFFFFFFLL)
            ids.insert(static_cast<quint32>(id));
    }
    if (ids.isEmpty())
        return;
    m_etag = reply->rawHeader("ETag");
    const bool differs = ids != m_ids;
    m_ids = std::move(ids);
    save();
    if (differs)
        emit changed();
}

void AdultListService::save() const
{
    QJsonArray arr;
    for (quint32 id : m_ids)
        arr.append(static_cast<double>(id));
    QJsonObject obj;
    obj.insert(QStringLiteral("version"), 1);
    obj.insert(QStringLiteral("etag"), QString::fromUtf8(m_etag));
    obj.insert(QStringLiteral("appIds"), arr);
    QSaveFile file(cacheFilePath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    file.commit();
}

} // namespace arachnel::core

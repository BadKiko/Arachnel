#include "catalog_disk_cache.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace arachnel::core {
namespace CatalogDiskCache {
namespace {

QString safeSourceFileName(const QString& sourceId)
{
    QString name = sourceId.trimmed();
    name.replace(QLatin1Char('/'), QLatin1Char('_'));
    name.replace(QLatin1Char('\\'), QLatin1Char('_'));
    name.replace(QLatin1Char(':'), QLatin1Char('_'));
    if (name.isEmpty())
        name = QStringLiteral("unknown");
    return name;
}

QString payloadFilePath(const QString& sourceId)
{
    return cacheDir() + QLatin1Char('/') + safeSourceFileName(sourceId) + QStringLiteral(".json");
}

QString metaFilePath(const QString& sourceId)
{
    return cacheDir() + QLatin1Char('/') + safeSourceFileName(sourceId) + QStringLiteral(".meta");
}

} // namespace

QString cacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/catalog-cache");
}

QByteArray payloadSha256(const QByteArray& payload)
{
    return QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex();
}

bool savePayload(const QString& sourceId, const QByteArray& payload, const QByteArray& etag)
{
    if (sourceId.isEmpty() || payload.isEmpty())
        return false;
    QDir().mkpath(cacheDir());
    const QString path = payloadFilePath(sourceId);
    const QString tmp = path + QStringLiteral(".tmp");
    {
        QFile file(tmp);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        if (file.write(payload) != payload.size()) {
            file.close();
            QFile::remove(tmp);
            return false;
        }
    }
    QFile::remove(path);
    if (!QFile::rename(tmp, path)) {
        QFile::remove(tmp);
        return false;
    }

    QFile meta(metaFilePath(sourceId));
    if (meta.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        meta.write(payloadSha256(payload));
        meta.write("\n");
        meta.write(etag);
        meta.write("\n");
        meta.write(QByteArray::number(QFileInfo(path).lastModified().toMSecsSinceEpoch()));
        meta.write("\n");
    }
    return true;
}

bool loadPayload(const QString& sourceId, QByteArray* payload, QByteArray* etag, qint64* savedAtMs)
{
    if (sourceId.isEmpty())
        return false;

    if (etag)
        etag->clear();
    if (savedAtMs)
        *savedAtMs = 0;

    QFile meta(metaFilePath(sourceId));
    if (meta.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QList<QByteArray> lines = meta.readAll().split('\n');
        if (etag && lines.size() >= 2)
            *etag = lines.at(1).trimmed();
        if (savedAtMs && lines.size() >= 3)
            *savedAtMs = lines.at(2).trimmed().toLongLong();
    }

    if (!payload)
        return QFile::exists(payloadFilePath(sourceId)) || (etag && !etag->isEmpty());

    QFile file(payloadFilePath(sourceId));
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return false;
    *payload = file.readAll();
    if (payload->isEmpty())
        return false;
    if (savedAtMs && *savedAtMs <= 0)
        *savedAtMs = QFileInfo(file).lastModified().toMSecsSinceEpoch();
    return true;
}

void remove(const QString& sourceId)
{
    if (sourceId.isEmpty())
        return;
    QFile::remove(payloadFilePath(sourceId));
    QFile::remove(metaFilePath(sourceId));
}

} // namespace CatalogDiskCache
} // namespace arachnel::core

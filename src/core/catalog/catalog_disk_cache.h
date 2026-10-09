#pragma once

#include <QByteArray>
#include <QString>

namespace arachnel::core {

/** Persist raw catalog JSON per source for fast relaunch (parse off network). */
namespace CatalogDiskCache {

QString cacheDir();
/** Pass `payloadSha` when the caller already hashed `payload`; an unchanged payload (same hash, etag and size on disk) is not rewritten. */
bool savePayload(const QString& sourceId, const QByteArray& payload, const QByteArray& etag,
                 const QByteArray& payloadSha = {});
bool loadPayload(const QString& sourceId, QByteArray* payload, QByteArray* etag = nullptr,
                 qint64* savedAtMs = nullptr);
QByteArray payloadSha256(const QByteArray& payload);
void remove(const QString& sourceId);

} // namespace CatalogDiskCache

} // namespace arachnel::core

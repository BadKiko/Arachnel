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
/**
 * Content fingerprint used to detect an unchanged feed (cache keys, "same payload" checks).
 * Not cryptographic: SHA-256 over a ~70 MB catalog cost ~300 ms on every launch.
 */
QByteArray payloadFingerprint(const QByteArray& payload);
/** Fingerprint recorded when the payload was last saved; empty when there is none. Reads only .meta. */
QByteArray storedPayloadKey(const QString& sourceId);
/** Rewrites only the key line of .meta (e.g. after the fingerprint format changed). */
void setStoredPayloadKey(const QString& sourceId, const QByteArray& key);
void remove(const QString& sourceId);

} // namespace CatalogDiskCache

} // namespace arachnel::core

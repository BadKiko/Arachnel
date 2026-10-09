#include "catalog_disk_cache.h"

#include <cstring>

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

QByteArray payloadFingerprint(const QByteArray& payload)
{
    // Four independent multiply-xor lanes over 32-byte blocks, then a final mix: several GB/s,
    // deterministic across runs and platforms (unlike qHash, which may use CPU-specific paths).
    constexpr quint64 kMul[4] = {0x9E3779B97F4A7C15ULL, 0xC2B2AE3D27D4EB4FULL,
                                 0x165667B19E3779F9ULL, 0x27D4EB2F165667C5ULL};
    quint64 lane[4] = {0x243F6A8885A308D3ULL, 0x13198A2E03707344ULL, 0xA4093822299F31D0ULL,
                       0x082EFA98EC4E6C89ULL};
    const char* p = payload.constData();
    qsizetype n = payload.size();
    const qsizetype total = n;
    const auto rotl = [](quint64 v, int r) { return (v << r) | (v >> (64 - r)); };
    while (n >= 32) {
        for (int i = 0; i < 4; ++i) {
            quint64 w;
            memcpy(&w, p + i * 8, 8);
            lane[i] = rotl((lane[i] ^ w) * kMul[i], 29) * kMul[(i + 1) & 3];
        }
        p += 32;
        n -= 32;
    }
    quint64 tail = 0;
    for (qsizetype i = 0; i < n; ++i)
        tail = (tail << 8) | static_cast<quint8>(p[i]);
    lane[0] ^= tail;
    quint64 a = lane[0] ^ rotl(lane[1], 13) ^ rotl(lane[2], 27) ^ rotl(lane[3], 41);
    quint64 b = lane[1] ^ rotl(lane[2], 17) ^ rotl(lane[3], 31) ^ rotl(lane[0], 47);
    for (quint64* v : {&a, &b}) {
        *v ^= *v >> 33;
        *v *= 0xFF51AFD7ED558CCDULL;
        *v ^= *v >> 33;
        *v *= 0xC4CEB9FE1A85EC53ULL;
        *v ^= *v >> 33;
    }
    return QByteArray::number(a, 16).rightJustified(16, '0')
        + QByteArray::number(b, 16).rightJustified(16, '0') + '-' + QByteArray::number(total);
}

QByteArray storedPayloadKey(const QString& sourceId)
{
    QFile meta(metaFilePath(sourceId));
    if (!meta.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return meta.readLine(256).trimmed();
}

void setStoredPayloadKey(const QString& sourceId, const QByteArray& key)
{
    QFile meta(metaFilePath(sourceId));
    if (!meta.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QList<QByteArray> lines = meta.readAll().split('\n');
    meta.close();
    if (lines.isEmpty())
        return;
    lines[0] = key;
    if (meta.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        meta.write(lines.join('\n'));
}

bool savePayload(const QString& sourceId, const QByteArray& payload, const QByteArray& etag,
                 const QByteArray& payloadSha)
{
    if (sourceId.isEmpty() || payload.isEmpty())
        return false;
    QDir().mkpath(cacheDir());
    const QString path = payloadFilePath(sourceId);
    const QByteArray sha = payloadSha.isEmpty() ? payloadFingerprint(payload) : payloadSha;

    // Plugin catalogs (steamidra is ~49 MB) are re-serialized on every launch and are almost
    // always identical to what is already on disk: skip rewriting the payload in that case.
    bool unchanged = false;
    {
        QFile oldMeta(metaFilePath(sourceId));
        if (oldMeta.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QList<QByteArray> lines = oldMeta.readAll().split('\n');
            unchanged = lines.size() >= 2 && lines.at(0).trimmed() == sha
                && lines.at(1).trimmed() == etag && QFileInfo(path).size() == payload.size();
        }
    }
    if (unchanged) {
        // The saved-at stamp in .meta comes from the file mtime; keep it fresh so a feed that
        // returned identical bytes still counts as just refreshed.
        QFile existing(path);
        if (existing.open(QIODevice::ReadWrite))
            existing.setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
    } else {
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
    }

    QFile meta(metaFilePath(sourceId));
    if (meta.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        meta.write(sha);
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

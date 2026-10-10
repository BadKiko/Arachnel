#include "catalog_snapshot.h"

#include "catalog_disk_cache.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QThread>
#include <QtConcurrent>

#include <cstring>

namespace arachnel::core {
namespace CatalogSnapshot {
namespace {

constexpr quint32 kMagic = 0x4E534341; // "ACSN" (host byte order: snapshots never leave the machine)
constexpr quint32 kEndMarker = 0xA11C0DED;
// Bump when the layout below changes. Adding a field to CatalogEntry/CatalogComponent also
// requires updating write/readEntry; the struct-size check in the header catches the rest.
constexpr quint32 kFormatVersion = 2;
constexpr int kBlockCount = 16; // independent blocks: encoded and decoded in parallel

QString snapshotPath(const QString& sourceId)
{
    QString name = sourceId.trimmed();
    name.replace(QLatin1Char('/'), QLatin1Char('_'));
    name.replace(QLatin1Char('\\'), QLatin1Char('_'));
    name.replace(QLatin1Char(':'), QLatin1Char('_'));
    if (name.isEmpty())
        name = QStringLiteral("unknown");
    return CatalogDiskCache::cacheDir() + QLatin1Char('/') + name + QStringLiteral(".snapshot");
}

// Identifies the build that wrote the snapshot: a new executable (update, rebuild) may derive
// entry fields differently, so it never reuses an older snapshot.
QByteArray buildIdentity()
{
    const QFileInfo exe(QCoreApplication::applicationFilePath());
    return QByteArray::number(exe.size()) + '/'
        + QByteArray::number(exe.lastModified().toMSecsSinceEpoch());
}

// ---- raw encoding (host byte order, strings as UTF-16) --------------------------------------

struct Writer
{
    QByteArray buf;

    template <typename T>
    void put(T v)
    {
        const qsizetype at = buf.size();
        buf.resize(at + static_cast<qsizetype>(sizeof(T)));
        memcpy(buf.data() + at, &v, sizeof(T));
    }
    void str(const QString& s)
    {
        const quint32 n = static_cast<quint32>(s.size());
        put(n);
        if (n) {
            const qsizetype at = buf.size();
            buf.resize(at + static_cast<qsizetype>(n) * 2);
            memcpy(buf.data() + at, s.constData(), static_cast<size_t>(n) * 2);
        }
    }
    void list(const QStringList& l)
    {
        put(static_cast<quint32>(l.size()));
        for (const QString& s : l)
            str(s);
    }
};

struct Reader
{
    const uchar* p = nullptr;
    const uchar* end = nullptr;
    bool ok = true;

    template <typename T>
    T get()
    {
        T v{};
        if (static_cast<size_t>(end - p) < sizeof(T)) {
            ok = false;
            return v;
        }
        memcpy(&v, p, sizeof(T));
        p += sizeof(T);
        return v;
    }
    QString str()
    {
        const quint32 n = get<quint32>();
        if (!ok || n == 0)
            return {};
        const size_t bytes = static_cast<size_t>(n) * 2;
        if (n > 0x3FFFFFFFu || static_cast<size_t>(end - p) < bytes) {
            ok = false;
            return {};
        }
        QString s(static_cast<qsizetype>(n), Qt::Uninitialized);
        memcpy(s.data(), p, bytes); // memcpy: the mapped data is not guaranteed 2-byte aligned
        p += bytes;
        return s;
    }
    QStringList list()
    {
        const quint32 n = get<quint32>();
        if (!ok || n == 0)
            return {};
        if (n > 1000000u) {
            ok = false;
            return {};
        }
        QStringList out;
        out.reserve(static_cast<qsizetype>(n));
        for (quint32 i = 0; i < n && ok; ++i)
            out.append(str());
        return out;
    }
};

void writeComponent(Writer& w, const CatalogComponent& c)
{
    w.str(c.id);
    w.str(c.title);
    w.list(c.magnetUris);
    w.str(c.downloadUrl);
    w.str(c.referer);
    w.str(c.getfileUrl);
    w.str(c.fileSize);
    w.str(c.uploadDate);
    w.str(c.coverUrl);
    w.list(c.screenshotUrls);
    w.put(static_cast<qint32>(c.kind));
    w.put(static_cast<qint32>(c.delivery));
    w.put(static_cast<quint8>(c.optional));
    w.put(static_cast<quint8>(c.contentAvailable));
}

void readComponent(Reader& r, CatalogComponent& c)
{
    c.id = r.str();
    c.title = r.str();
    c.magnetUris = r.list();
    c.downloadUrl = r.str();
    c.referer = r.str();
    c.getfileUrl = r.str();
    c.fileSize = r.str();
    c.uploadDate = r.str();
    c.coverUrl = r.str();
    c.screenshotUrls = r.list();
    c.kind = static_cast<CatalogItemKind>(r.get<qint32>());
    c.delivery = static_cast<ComponentDelivery>(r.get<qint32>());
    c.optional = r.get<quint8>() != 0;
    c.contentAvailable = r.get<quint8>() != 0;
}

void writeEntry(Writer& w, const CatalogEntry& e)
{
    w.str(e.id);
    w.str(e.title);
    w.str(e.coverUrl);
    w.str(e.remoteCoverUrl);
    w.str(e.sourceId);
    w.str(e.sourcePageUrl);
    w.str(e.version);
    w.str(e.sizeLabel);
    w.str(e.description);
    w.str(e.genres);
    w.str(e.steamAppId);
    w.str(e.trailerUrl);
    w.str(e.trailerThumbnailUrl);
    w.list(e.screenshotUrls);
    w.put(static_cast<qint32>(e.installKind));
    w.list(e.magnetUris);
    w.str(e.uploadDate);
    w.str(e.parentEntryId);
    w.put(static_cast<qint32>(e.itemKind));
    w.put(static_cast<quint32>(e.addons.size()));
    for (const CatalogComponent& c : e.addons)
        writeComponent(w, c);
    w.put(static_cast<quint8>(e.metadataPending));
    w.put(static_cast<quint8>(e.hasWorkshop));
    w.put(static_cast<qint32>(e.dlcCount));
    w.str(e.titleLower);
    w.put(static_cast<qint64>(e.sizeBytes));
    w.put(static_cast<qint64>(e.uploadDay));
    w.put(static_cast<quint32>(e.genreBits));
    w.put(static_cast<quint8>(e.playModeMask));
    w.put(static_cast<quint8>(e.hasDrm));
    w.put(static_cast<qint32>(e.recommendationsTotal));
    w.put(static_cast<qint32>(e.metacriticScore));
    w.put(static_cast<qint64>(e.releaseDay));
    w.put(static_cast<qint32>(e.currentPlayers));
    w.put(static_cast<qint64>(e.playersFetchedAt));
    w.put(static_cast<double>(e.hypeScore));
}

bool readEntry(Reader& r, CatalogEntry& e)
{
    e.id = r.str();
    e.title = r.str();
    e.coverUrl = r.str();
    e.remoteCoverUrl = r.str();
    e.sourceId = r.str();
    e.sourcePageUrl = r.str();
    e.version = r.str();
    e.sizeLabel = r.str();
    e.description = r.str();
    e.genres = r.str();
    e.steamAppId = r.str();
    e.trailerUrl = r.str();
    e.trailerThumbnailUrl = r.str();
    e.screenshotUrls = r.list();
    e.installKind = static_cast<InstallKind>(r.get<qint32>());
    e.magnetUris = r.list();
    e.uploadDate = r.str();
    e.parentEntryId = r.str();
    e.itemKind = static_cast<CatalogItemKind>(r.get<qint32>());
    const quint32 addonCount = r.get<quint32>();
    if (!r.ok || addonCount > 100000u)
        return false;
    if (addonCount) {
        e.addons.resize(static_cast<qsizetype>(addonCount));
        for (CatalogComponent& c : e.addons)
            readComponent(r, c);
    }
    e.metadataPending = r.get<quint8>() != 0;
    e.hasWorkshop = r.get<quint8>() != 0;
    e.dlcCount = r.get<qint32>();
    e.titleLower = r.str();
    e.sizeBytes = r.get<qint64>();
    e.uploadDay = r.get<qint64>();
    e.genreBits = r.get<quint32>();
    e.playModeMask = r.get<quint8>();
    e.hasDrm = r.get<quint8>() != 0;
    e.recommendationsTotal = r.get<qint32>();
    e.metacriticScore = r.get<qint32>();
    e.releaseDay = r.get<qint64>();
    e.currentPlayers = r.get<qint32>();
    e.playersFetchedAt = r.get<qint64>();
    e.hypeScore = r.get<double>();
    return r.ok;
}

struct BlockSpan
{
    qsizetype first = 0;
    qsizetype count = 0;
    QByteArray encoded; // filled by the encode pass
};

} // namespace

bool save(const QString& sourceId, const QByteArray& payloadKey,
          const QVector<CatalogEntry>& entries)
{
    if (sourceId.isEmpty() || payloadKey.isEmpty() || entries.isEmpty())
        return false;

    const qsizetype total = entries.size();
    const int blocks = static_cast<int>(qMin<qsizetype>(kBlockCount, total));
    QVector<BlockSpan> spans(blocks);
    for (int i = 0; i < blocks; ++i) {
        spans[i].first = total * i / blocks;
        spans[i].count = total * (i + 1) / blocks - spans[i].first;
    }
    // Each block is encoded independently, so the work spreads across cores.
    QtConcurrent::blockingMap(spans, [&entries](BlockSpan& span) {
        Writer w;
        w.buf.reserve(static_cast<qsizetype>(span.count) * 560);
        for (qsizetype i = 0; i < span.count; ++i)
            writeEntry(w, entries.at(span.first + i));
        span.encoded = std::move(w.buf);
    });

    Writer head;
    head.put(kMagic);
    head.put(kFormatVersion);
    head.put(static_cast<quint64>(sizeof(CatalogEntry)));
    const QByteArray build = buildIdentity();
    head.put(static_cast<quint32>(build.size()));
    head.buf.append(build);
    head.put(static_cast<quint32>(payloadKey.size()));
    head.buf.append(payloadKey);
    head.put(static_cast<quint32>(total));
    head.put(static_cast<quint32>(blocks));
    for (const BlockSpan& span : std::as_const(spans)) {
        head.put(static_cast<quint32>(span.count));
        head.put(static_cast<quint64>(span.encoded.size()));
    }

    QDir().mkpath(CatalogDiskCache::cacheDir());
    QSaveFile file(snapshotPath(sourceId));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    bool ok = file.write(head.buf) == head.buf.size();
    for (const BlockSpan& span : std::as_const(spans)) {
        if (!ok)
            break;
        ok = file.write(span.encoded) == span.encoded.size();
    }
    Writer tail;
    tail.put(kEndMarker);
    ok = ok && file.write(tail.buf) == tail.buf.size();
    if (!ok) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

bool load(const QString& sourceId, const QByteArray& payloadKey, QVector<CatalogEntry>* out)
{
    if (!out || sourceId.isEmpty() || payloadKey.isEmpty())
        return false;

    QFile file(snapshotPath(sourceId));
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const qint64 size = file.size();
    if (size < 64)
        return false;
    // Map instead of readAll(): no second copy of a ~100 MB file on top of the entries.
    const uchar* mapped = file.map(0, size);
    if (!mapped)
        return false;

    Reader r;
    r.p = mapped;
    r.end = mapped + size;
    if (r.get<quint32>() != kMagic || r.get<quint32>() != kFormatVersion
        || r.get<quint64>() != static_cast<quint64>(sizeof(CatalogEntry))) {
        return false;
    }
    const quint32 buildLen = r.get<quint32>();
    if (!r.ok || buildLen > 256 || static_cast<size_t>(r.end - r.p) < buildLen)
        return false;
    const QByteArray build(reinterpret_cast<const char*>(r.p), static_cast<qsizetype>(buildLen));
    r.p += buildLen;
    const quint32 keyLen = r.get<quint32>();
    if (!r.ok || keyLen > 256 || static_cast<size_t>(r.end - r.p) < keyLen)
        return false;
    const QByteArray key(reinterpret_cast<const char*>(r.p), static_cast<qsizetype>(keyLen));
    r.p += keyLen;
    const quint32 count = r.get<quint32>();
    const quint32 blockCount = r.get<quint32>();
    if (!r.ok || build != buildIdentity() || key != payloadKey || count == 0
        || count > 5000000u || blockCount == 0 || blockCount > 256) {
        return false;
    }

    struct Block
    {
        qsizetype first = 0;
        quint32 count = 0;
        const uchar* data = nullptr;
        quint64 bytes = 0;
        bool ok = false;
    };
    QVector<Block> blocks(static_cast<qsizetype>(blockCount));
    quint64 declaredEntries = 0;
    for (Block& b : blocks) {
        b.count = r.get<quint32>();
        b.bytes = r.get<quint64>();
        b.first = static_cast<qsizetype>(declaredEntries);
        declaredEntries += b.count;
    }
    if (!r.ok || declaredEntries != count)
        return false;
    const uchar* cursor = r.p;
    for (Block& b : blocks) {
        if (static_cast<quint64>(r.end - cursor) < b.bytes)
            return false;
        b.data = cursor;
        cursor += b.bytes;
    }
    if (static_cast<size_t>(r.end - cursor) < sizeof(quint32))
        return false;
    quint32 marker = 0;
    memcpy(&marker, cursor, sizeof(marker));
    if (marker != kEndMarker)
        return false;

    QVector<CatalogEntry> entries;
    entries.resize(static_cast<qsizetype>(count));
    CatalogEntry* dst = entries.data();
    QtConcurrent::blockingMap(blocks, [dst](Block& b) {
        Reader br;
        br.p = b.data;
        br.end = b.data + b.bytes;
        b.ok = true;
        for (quint32 i = 0; i < b.count && b.ok; ++i)
            b.ok = readEntry(br, dst[b.first + i]);
        // A block must be consumed exactly: leftover bytes mean a layout mismatch.
        b.ok = b.ok && br.p == br.end;
    });
    for (const Block& b : std::as_const(blocks)) {
        if (!b.ok)
            return false;
    }

    *out = std::move(entries);
    return true;
}

void remove(const QString& sourceId)
{
    QFile::remove(snapshotPath(sourceId));
}

bool exists(const QString& sourceId)
{
    return QFileInfo::exists(snapshotPath(sourceId));
}

} // namespace CatalogSnapshot
} // namespace arachnel::core

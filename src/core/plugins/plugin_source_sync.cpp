#include "plugin_source_sync.h"

#include "catalog_disk_cache.h"
#include "catalog_snapshot.h"
#include "plugin_api.h"
#include "plugin_catalog_json.h"
#include "plugin_host.h"
#include "crash_log.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QUrl>
#include <QtConcurrent>

#include <algorithm>

namespace arachnel::core {

namespace {

constexpr qsizetype kRowsPerBatch = 1024;
// First-start preview: ~600 KB of a 70 MB feed is ~1000 newest-first rows, enough to fill the
// grid while the remaining ~99% downloads.
constexpr qsizetype kPreviewPrefixBytes = 600'000;
constexpr qsizetype kPreviewMaxRows = 1024;

struct SourceDescriptor {
    QUrl url;
    QByteArrayList rowsKeys;
    qint64 ttlSeconds = 0;
    int minRows = 1;
    bool parallel = false;
    QByteArray userAgent = "Arachnel/0.1";
    QList<QPair<QByteArray, QByteArray>> headers;
    bool valid = false;
};

SourceDescriptor parseDescriptor(const QByteArray& json)
{
    SourceDescriptor d;
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    const QString url = o.value(QStringLiteral("url")).toString().trimmed();
    d.url = QUrl(url);
    if (!d.url.isValid()
        || (d.url.scheme() != QLatin1String("http") && d.url.scheme() != QLatin1String("https")))
        return d;
    for (const QJsonValue& k : o.value(QStringLiteral("rowsKeys")).toArray())
        d.rowsKeys.append(k.toString().toUtf8());
    d.ttlSeconds = std::max<qint64>(0, static_cast<qint64>(o.value(QStringLiteral("ttlSeconds")).toDouble(0)));
    d.minRows = std::max(1, o.value(QStringLiteral("minRows")).toInt(1));
    d.parallel = o.value(QStringLiteral("parallel")).toBool(false);
    const QString ua = o.value(QStringLiteral("userAgent")).toString().trimmed();
    if (!ua.isEmpty())
        d.userAgent = ua.toUtf8();
    const QJsonObject headers = o.value(QStringLiteral("headers")).toObject();
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
        d.headers.append({it.key().toUtf8(), it.value().toString().toUtf8()});
    d.valid = true;
    return d;
}

// ---- minimal JSON scanner: finds value boundaries without building a tree ----------------

qsizetype skipValue(const char* p, qsizetype n, qsizetype i)
{
    if (i >= n)
        return -1;
    const char c = p[i];
    if (c == '"') {
        for (++i; i < n;) {
            if (p[i] == '\\')
                i += 2;
            else if (p[i] == '"')
                return i + 1;
            else
                ++i;
        }
        return -1;
    }
    if (c == '{' || c == '[') {
        int depth = 0;
        bool inString = false;
        for (; i < n; ++i) {
            const char ch = p[i];
            if (inString) {
                if (ch == '\\')
                    ++i;
                else if (ch == '"')
                    inString = false;
            } else if (ch == '"') {
                inString = true;
            } else if (ch == '{' || ch == '[') {
                ++depth;
            } else if (ch == '}' || ch == ']') {
                if (--depth == 0)
                    return i + 1;
            }
        }
        return -1;
    }
    while (i < n && p[i] != ',' && p[i] != ']' && p[i] != '}' && p[i] != ' ' && p[i] != '\n'
           && p[i] != '\r' && p[i] != '\t') {
        ++i;
    }
    return i;
}

qsizetype skipSpace(const char* p, qsizetype n, qsizetype i)
{
    while (i < n && (p[i] == ' ' || p[i] == '\n' || p[i] == '\r' || p[i] == '\t'))
        ++i;
    return i;
}

struct Range {
    qsizetype begin = -1; // just after '['
    qsizetype end = -1;   // at the matching ']'
    bool valid() const { return begin >= 0 && end >= begin; }
};

// Rows array: a top-level array, or the first non-empty one among `keys` in a top-level object.
bool locateRows(const char* p, qsizetype n, const QByteArrayList& keys, Range* out)
{
    qsizetype i = skipSpace(p, n, 0);
    if (i >= n)
        return false;
    const auto rangeOfArray = [&](qsizetype at) {
        Range r;
        const qsizetype stop = skipValue(p, n, at);
        if (stop > at) {
            r.begin = at + 1;
            r.end = stop - 1;
        }
        return r;
    };
    if (p[i] == '[') {
        *out = rangeOfArray(i);
        return out->valid();
    }
    if (p[i] != '{')
        return false;
    QVector<Range> found(keys.size());
    ++i;
    for (;;) {
        i = skipSpace(p, n, i);
        if (i >= n)
            return false;
        if (p[i] == '}')
            break;
        if (p[i] == ',') {
            ++i;
            continue;
        }
        if (p[i] != '"')
            return false;
        const qsizetype keyEnd = skipValue(p, n, i);
        if (keyEnd < 0)
            return false;
        const QByteArrayView key(p + i + 1, keyEnd - i - 2);
        i = skipSpace(p, n, keyEnd);
        if (i >= n || p[i] != ':')
            return false;
        i = skipSpace(p, n, i + 1);
        if (i >= n)
            return false;
        const qsizetype valueEnd = skipValue(p, n, i);
        if (valueEnd < 0)
            return false;
        if (p[i] == '[') {
            for (qsizetype k = 0; k < keys.size(); ++k) {
                if (key == QByteArrayView(keys.at(k)))
                    found[k] = rangeOfArray(i);
            }
        }
        i = valueEnd;
    }
    for (const Range& r : found) {
        if (r.valid() && skipSpace(p, n, r.begin) < r.end) {
            *out = r;
            return true;
        }
    }
    return false;
}

struct RowSpan {
    qsizetype begin;
    qsizetype end;
};

bool collectRowSpans(const char* p, qsizetype n, const Range& range, QVector<RowSpan>* spans)
{
    qsizetype i = range.begin;
    for (;;) {
        i = skipSpace(p, n, i);
        if (i >= range.end)
            return true;
        if (p[i] == ',') {
            ++i;
            continue;
        }
        const qsizetype stop = skipValue(p, n, i);
        if (stop < 0 || stop > range.end)
            return false;
        if (p[i] == '{')
            spans->append({i, stop});
        i = stop;
    }
}

// ---- download ----------------------------------------------------------------------------

enum class FetchStatus { Ok, NotModified, Error };

FetchStatus fetchToFile(const SourceDescriptor& d, const QByteArray& etag, const QString& tmpPath,
                        QByteArray* newEtag, QString* error,
                        const std::function<void(const QByteArray&)>& onPrefix = {})
{
    QNetworkAccessManager nam;
    QNetworkRequest req(d.url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader, d.userAgent);
    req.setTransferTimeout(30000);
    // No Accept-Encoding override: Qt negotiates gzip/deflate and inflates transparently.
    for (const auto& h : d.headers)
        req.setRawHeader(h.first, h.second);
    if (!etag.isEmpty())
        req.setRawHeader("If-None-Match", etag);

    QFile out(tmpPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *error = QStringLiteral("cannot write %1").arg(tmpPath);
        return FetchStatus::Error;
    }

    QNetworkReply* reply = nam.get(req);
    bool writeFailed = false;
    QByteArray prefix;
    bool prefixSent = !onPrefix;
    QObject::connect(reply, &QNetworkReply::readyRead, reply, [&]() {
        const QByteArray chunk = reply->readAll();
        if (out.write(chunk) != chunk.size())
            writeFailed = true;
        if (!prefixSent) {
            prefix.append(chunk);
            if (prefix.size() >= kPreviewPrefixBytes && reply->error() == QNetworkReply::NoError) {
                prefixSent = true;
                onPrefix(prefix);
                prefix = QByteArray();
            }
        }
    });
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netError = reply->error();
    const QString netErrorText = reply->errorString();
    if (netError == QNetworkReply::NoError)
        out.write(reply->readAll());
    *newEtag = reply->rawHeader("ETag");
    reply->deleteLater();
    out.close();

    if (status == 304) {
        QFile::remove(tmpPath);
        return FetchStatus::NotModified;
    }
    if (netError != QNetworkReply::NoError || writeFailed || QFileInfo(tmpPath).size() <= 0) {
        QFile::remove(tmpPath);
        *error = netError != QNetworkReply::NoError ? netErrorText
                                                    : QStringLiteral("empty or truncated download");
        return FetchStatus::Error;
    }
    return FetchStatus::Ok;
}

// Rows of the feed's array that are complete inside `prefix` (the start of the download).
QByteArray previewRowsJson(const QByteArray& prefix, const QByteArrayList& rowsKeys)
{
    const char* p = prefix.constData();
    const qsizetype n = prefix.size();
    qsizetype i = skipSpace(p, n, 0);
    if (i >= n)
        return {};
    if (p[i] != '[') {
        qsizetype at = -1;
        for (const QByteArray& key : rowsKeys) {
            const QByteArray needle = '"' + key + '"';
            qsizetype k = prefix.indexOf(needle);
            while (k >= 0) {
                qsizetype j = skipSpace(p, n, k + needle.size());
                if (j < n && p[j] == ':') {
                    j = skipSpace(p, n, j + 1);
                    if (j < n && p[j] == '[') {
                        at = j;
                        break;
                    }
                }
                k = prefix.indexOf(needle, k + 1);
            }
            if (at >= 0)
                break;
        }
        if (at < 0)
            return {};
        i = at;
    }
    ++i; // past '['

    QByteArray rows("[");
    qsizetype count = 0;
    while (count < kPreviewMaxRows) {
        i = skipSpace(p, n, i);
        if (i >= n)
            break;
        if (p[i] == ',') {
            ++i;
            continue;
        }
        if (p[i] != '{')
            break;
        const qsizetype stop = skipValue(p, n, i);
        if (stop < 0)
            break; // the row is cut off by the end of the prefix
        if (count > 0)
            rows.append(',');
        rows.append(p + i, stop - i);
        ++count;
        i = stop;
    }
    if (count == 0)
        return {};
    rows.append(']');
    return rows;
}

QByteArray keySuffix(const QString& pluginVersion)
{
    return '|' + pluginVersion.toUtf8() + "|x" + QByteArray::number(ARACHNEL_PLUGIN_SOURCE_EXT_VERSION);
}

QByteArray keyPrefix(const QByteArray& key)
{
    const qsizetype bar = key.indexOf('|');
    return bar < 0 ? key : key.left(bar);
}

QString pluginVersionOf(const PluginHost& host, const QString& id)
{
    for (const SourcePluginInfo& info : host.pluginInfos()) {
        if (info.id == id)
            return info.pluginVersion;
    }
    return {};
}

} // namespace

bool pluginCatalogSourceEnabled()
{
    return !qEnvironmentVariableIsSet("ARACHNEL_DISABLE_PLUGIN_SOURCE");
}

QByteArray pluginCatalogSourceKeySuffix(const PluginHost& host, const QString& sourceId)
{
    if (!pluginCatalogSourceEnabled() || !host.pluginHasCatalogSource(sourceId))
        return {};
    return keySuffix(pluginVersionOf(host, sourceId));
}

PluginSourceSyncResult syncPluginCatalogSource(const PluginHost& host, const QString& sourceId,
                                               const QByteArray& knownKey, bool force,
                                               const PluginSourcePreview& onPreview)
{
    PluginSourceSyncResult result;
    if (!pluginCatalogSourceEnabled() || !host.pluginHasCatalogSource(sourceId)) {
        result.error = QStringLiteral("catalog source not available");
        return result;
    }
    const SourceDescriptor desc = parseDescriptor(host.pluginCatalogSourceDescriptor(sourceId));
    if (!desc.valid) {
        result.error = QStringLiteral("invalid catalog source descriptor");
        return result;
    }

    const QByteArray suffix = keySuffix(pluginVersionOf(host, sourceId));
    const QString rawPath = CatalogDiskCache::sidecarPath(sourceId, QStringLiteral(".src"));
    const QString tmpPath = rawPath + QStringLiteral(".tmp");
    QDir().mkpath(CatalogDiskCache::cacheDir());

    QByteArray storedKey = CatalogDiskCache::storedPayloadKey(sourceId);
    QByteArray etag;
    qint64 savedAtMs = 0;
    CatalogDiskCache::loadPayload(sourceId, nullptr, &etag, &savedAtMs);
    const bool haveRaw = QFileInfo(rawPath).size() > 0 && storedKey.contains('|');
    if (!haveRaw)
        etag.clear(); // an etag without the bytes it describes would only produce a useless 304

    const auto touchMeta = [&](const QByteArray& key, const QByteArray& tag) {
        CatalogDiskCache::saveMeta(sourceId, key, tag);
    };

    // 1. Is the cached raw feed fresh enough to skip the network?
    bool needNetwork = force || !haveRaw;
    if (!needNetwork && desc.ttlSeconds > 0) {
        const qint64 ageMs = QDateTime::currentMSecsSinceEpoch() - savedAtMs;
        needNetwork = savedAtMs <= 0 || ageMs < 0 || ageMs >= desc.ttlSeconds * 1000;
    } else if (!needNetwork) {
        needNetwork = true; // ttl 0: always ask (cheap with an ETag)
    }

    QByteArray newEtag = etag;
    bool rawChanged = false;
    bool networkOk = false;
    if (needNetwork) {
        QString err;
        QByteArray fetchedEtag;
        std::function<void(const QByteArray&)> onPrefix;
        if (onPreview && !haveRaw) {
            onPrefix = [&](const QByteArray& prefix) {
                const QByteArray rows = previewRowsJson(prefix, desc.rowsKeys);
                if (rows.isEmpty())
                    return;
                const QByteArray normalized = host.pluginNormalizeRows(
                    sourceId, rows.constData(), static_cast<size_t>(rows.size()));
                if (normalized.isEmpty())
                    return;
                QStringList supersedes;
                QVector<CatalogEntry> entries = parsePluginCatalogJsonEx(normalized, sourceId, &supersedes);
                if (!supersedes.isEmpty()) {
                    const QSet<QString> hidden(supersedes.begin(), supersedes.end());
                    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                                 [&hidden](const CatalogEntry& e) {
                                                     return hidden.contains(e.id);
                                                 }),
                                  entries.end());
                }
                if (!entries.isEmpty())
                    onPreview(std::move(entries));
            };
        }
        const FetchStatus status = fetchToFile(desc, etag, tmpPath, &fetchedEtag, &err, onPrefix);
        if (status == FetchStatus::Ok) {
            QFile::remove(rawPath);
            if (!QFile::rename(tmpPath, rawPath)) {
                QFile::remove(tmpPath);
                result.error = QStringLiteral("cannot store downloaded catalog");
                return result;
            }
            newEtag = fetchedEtag;
            rawChanged = true;
            networkOk = true;
        } else if (status == FetchStatus::NotModified) {
            networkOk = true;
        } else if (!haveRaw) {
            result.error = err;
            return result;
        }
        // Network error with a cached raw feed: keep working from it (offline), but do not
        // stamp it fresh so the next refresh tries again.
    }

    // 2. Same content as what the caller already shows?
    if (!rawChanged && haveRaw && storedKey.endsWith(suffix)) {
        if (networkOk)
            touchMeta(storedKey, newEtag);
        if (storedKey == knownKey) {
            result.status = PluginSourceSyncResult::Status::Unchanged;
            result.payloadKey = storedKey;
            return result;
        }
        // Snapshot written for this exact raw feed and plugin version.
        if (CatalogSnapshot::load(sourceId, storedKey, &result.entries)) {
            result.status = PluginSourceSyncResult::Status::Loaded;
            result.payloadKey = storedKey;
            return result;
        }
        result.entries.clear();
    }

    // 3. Map the raw feed, fingerprint it, normalize.
    QFile raw(rawPath);
    if (!raw.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("cannot read cached catalog");
        return result;
    }
    const qint64 rawSize = raw.size();
    uchar* mapped = raw.map(0, rawSize);
    QByteArray fallbackBuffer;
    const char* data = nullptr;
    if (mapped) {
        data = reinterpret_cast<const char*>(mapped);
    } else {
        fallbackBuffer = raw.readAll();
        data = fallbackBuffer.constData();
    }
    const qsizetype n = mapped ? static_cast<qsizetype>(rawSize) : fallbackBuffer.size();

    const QByteArray fingerprint =
        CatalogDiskCache::payloadFingerprint(QByteArray::fromRawData(data, n));
    const QByteArray key = fingerprint + suffix;

    if (key == knownKey) {
        if (networkOk)
            touchMeta(key, newEtag);
        if (mapped)
            raw.unmap(mapped);
        result.status = PluginSourceSyncResult::Status::Unchanged;
        result.payloadKey = key;
        return result;
    }
    if (!rawChanged && CatalogSnapshot::load(sourceId, key, &result.entries)) {
        if (mapped)
            raw.unmap(mapped);
        if (networkOk)
            touchMeta(key, newEtag);
        result.status = PluginSourceSyncResult::Status::Loaded;
        result.payloadKey = key;
        return result;
    }
    result.entries.clear();

    Range range;
    QVector<RowSpan> spans;
    const bool scanned = locateRows(data, n, desc.rowsKeys, &range)
        && collectRowSpans(data, n, range, &spans);
    if (!scanned || spans.isEmpty()) {
        if (mapped)
            raw.unmap(mapped);
        result.error = QStringLiteral("unexpected catalog layout");
        return result;
    }

    struct Batch {
        qsizetype first;
        qsizetype last; // exclusive
    };
    QVector<Batch> batches;
    for (qsizetype i = 0; i < spans.size(); i += kRowsPerBatch)
        batches.append({i, std::min<qsizetype>(i + kRowsPerBatch, spans.size())});

    struct BatchOut {
        bool ok = false;
        QVector<CatalogEntry> entries;
        QStringList supersedes;
    };
    const auto runBatch = [&](const Batch& b) {
        BatchOut out;
        QByteArray input;
        qsizetype total = 2;
        for (qsizetype i = b.first; i < b.last; ++i)
            total += spans.at(i).end - spans.at(i).begin + 1;
        input.reserve(total);
        input.append('[');
        for (qsizetype i = b.first; i < b.last; ++i) {
            if (i > b.first)
                input.append(',');
            input.append(data + spans.at(i).begin, spans.at(i).end - spans.at(i).begin);
        }
        input.append(']');
        const QByteArray normalized =
            host.pluginNormalizeRows(sourceId, input.constData(), static_cast<size_t>(input.size()));
        if (normalized.isEmpty())
            return out;
        out.entries = parsePluginCatalogJsonEx(normalized, sourceId, &out.supersedes);
        out.ok = true;
        return out;
    };

    QVector<BatchOut> outs;
    if (desc.parallel && batches.size() > 1) {
        outs = QtConcurrent::blockingMapped<QVector<BatchOut>>(
            batches, [&](const Batch& b) { return runBatch(b); });
    } else {
        outs.reserve(batches.size());
        for (const Batch& b : batches)
            outs.append(runBatch(b));
    }
    if (mapped)
        raw.unmap(mapped);
    raw.close();

    QSet<QString> superseded;
    qsizetype total = 0;
    for (const BatchOut& o : outs) {
        if (!o.ok) {
            result.error = QStringLiteral("plugin could not normalize the catalog");
            return result;
        }
        total += o.entries.size();
        for (const QString& id : o.supersedes)
            superseded.insert(id);
    }

    result.entries.reserve(total);
    for (BatchOut& o : outs) {
        for (CatalogEntry& e : o.entries) {
            if (!superseded.isEmpty() && superseded.contains(e.id))
                continue;
            result.entries.append(std::move(e));
        }
        o.entries.clear();
    }
    outs.clear();

    if (result.entries.size() < desc.minRows) {
        result.error = QStringLiteral("catalog has too few rows (%1)").arg(result.entries.size());
        result.entries.clear();
        return result;
    }

    // Snapshot first, then the meta key: the two never disagree.
    CatalogSnapshot::save(sourceId, key, result.entries);
    CatalogDiskCache::saveMeta(sourceId, key, newEtag);

    // A stale payload from the catalog_json path must not outlive the new raw feed.
    QFile::remove(CatalogDiskCache::sidecarPath(sourceId, QStringLiteral(".json")));

    result.status = PluginSourceSyncResult::Status::Loaded;
    result.payloadKey = key;
    return result;
}

} // namespace arachnel::core

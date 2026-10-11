#include "trailer_cache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>

namespace arachnel::core {

namespace {
constexpr qint64 kMaxTrailerBytes = 40LL * 1024 * 1024;   // a store "highlight" is a few MB
constexpr qint64 kMaxHlsBytes = 160LL * 1024 * 1024;      // a 720p trailer is 15-40 MB
constexpr qint64 kCacheBudgetBytes = 512LL * 1024 * 1024;
constexpr int kHlsParallel = 8;
constexpr int kTargetHeight = 720;

bool isHttpUrl(const QString& url)
{
    return url.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || url.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
}

// DASH: a manifest, not a file. Saving it would leave a broken ".mp4"; it streams instead.
bool isDash(const QString& url)
{
    return QUrl(url).path().endsWith(QStringLiteral(".mpd"), Qt::CaseInsensitive);
}

QString attribute(const QString& line, const QString& name)
{
    const QRegularExpression re(name + QStringLiteral("=(\"([^\"]*)\"|([^,]*))"));
    const QRegularExpressionMatch m = re.match(line);
    if (!m.hasMatch())
        return {};
    return m.captured(2).isEmpty() ? m.captured(3) : m.captured(2);
}

QString masterDirPath(const QUrl& masterUrl)
{
    const QString path = masterUrl.path();
    return path.left(path.lastIndexOf(QLatin1Char('/')) + 1);
}

// The file's place under the master playlist's folder; empty when it would leave that folder.
QString relativeToMaster(const QUrl& masterUrl, const QUrl& playlistUrl, const QString& uri)
{
    const QUrl resolved = playlistUrl.resolved(QUrl(uri));
    if (resolved.host() != masterUrl.host() || resolved.scheme() != masterUrl.scheme())
        return {};
    const QString base = masterDirPath(masterUrl);
    const QString path = QDir::cleanPath(resolved.path());
    if (!path.startsWith(base))
        return {};
    const QString rel = path.mid(base.size());
    if (rel.isEmpty() || rel.contains(QStringLiteral("..")) || rel.contains(QLatin1Char(':'))
        || rel.contains(QLatin1Char('\\')))
        return {};
    return rel;
}

qint64 entrySize(const QFileInfo& info)
{
    if (!info.isDir())
        return info.size();
    qint64 total = 0;
    QDirIterator it(info.absoluteFilePath(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}
} // namespace

TrailerCache::TrailerCache(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

bool TrailerCache::isHls(const QString& url)
{
    return QUrl(url).path().endsWith(QStringLiteral(".m3u8"), Qt::CaseInsensitive);
}

QString TrailerCache::cacheDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/trailer-cache");
}

QString TrailerCache::filePathFor(const QString& remoteUrl) const
{
    const QByteArray hash =
        QCryptographicHash::hash(remoteUrl.toUtf8(), QCryptographicHash::Sha1).toHex();
    const bool webm = QUrl(remoteUrl).path().endsWith(QStringLiteral(".webm"), Qt::CaseInsensitive);
    return cacheDir() + QLatin1Char('/') + QString::fromLatin1(hash)
        + (webm ? QStringLiteral(".webm") : QStringLiteral(".mp4"));
}

QString TrailerCache::hlsDirFor(const QString& remoteUrl) const
{
    // The token in the query changes; the trailer behind the path does not.
    QUrl url(remoteUrl);
    url.setQuery(QString());
    const QByteArray hash =
        QCryptographicHash::hash(url.toString().toUtf8(), QCryptographicHash::Sha1).toHex();
    return cacheDir() + QStringLiteral("/hls-") + QString::fromLatin1(hash);
}

QString TrailerCache::localUrlFor(const QString& remoteUrl) const
{
    if (!isHttpUrl(remoteUrl) || isDash(remoteUrl))
        return {};
    // The master playlist is written last, so its presence means every piece is there.
    const QString path = isHls(remoteUrl) ? hlsDirFor(remoteUrl) + QStringLiteral("/master.m3u8")
                                          : filePathFor(remoteUrl);
    return QFileInfo(path).size() > 0 ? QUrl::fromLocalFile(path).toString() : QString();
}

QNetworkReply* TrailerCache::get(const QUrl& url)
{
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return m_network->get(request);
}

void TrailerCache::prefetch(const QString& remoteUrl)
{
    if (!isHttpUrl(remoteUrl) || isDash(remoteUrl) || m_inFlight.contains(remoteUrl)
        || !localUrlFor(remoteUrl).isEmpty())
        return;

    m_inFlight.insert(remoteUrl);
    if (isHls(remoteUrl)) {
        startHls(remoteUrl);
        return;
    }

    QNetworkReply* reply = get(QUrl(remoteUrl));
    reply->setProperty("remoteUrl", remoteUrl);
    // Refuse anything unexpectedly large before it fills memory and disk.
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64 total) {
        if (received > kMaxTrailerBytes || total > kMaxTrailerBytes)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleFinished(reply); });
}

void TrailerCache::handleFinished(QNetworkReply* reply)
{
    const QString remoteUrl = reply->property("remoteUrl").toString();
    m_inFlight.remove(remoteUrl);
    const bool ok = reply->error() == QNetworkReply::NoError
        && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() < 400;
    const QByteArray payload = ok ? reply->readAll() : QByteArray();
    reply->deleteLater();
    if (payload.isEmpty()) {
        emit failed(remoteUrl);
        return;
    }

    QDir().mkpath(cacheDir());
    const QString path = filePathFor(remoteUrl);
    // Write beside, then rename: a half-written file must never look like a finished clip.
    const QString tmp = path + QStringLiteral(".part");
    QFile file(tmp);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit failed(remoteUrl);
        return;
    }
    const bool written = file.write(payload) == payload.size();
    file.close();
    if (!written) {
        QFile::remove(tmp);
        emit failed(remoteUrl);
        return;
    }
    QFile::remove(path);
    if (!QFile::rename(tmp, path)) {
        QFile::remove(tmp);
        emit failed(remoteUrl);
        return;
    }
    trimToBudget();
    emit ready(remoteUrl, QUrl::fromLocalFile(path).toString());
}

// ---------- HLS ----------

void TrailerCache::startHls(const QString& remoteUrl)
{
    HlsJob job;
    job.remoteUrl = remoteUrl;
    job.masterUrl = QUrl(remoteUrl);
    job.dir = hlsDirFor(remoteUrl);
    QDir(job.dir).removeRecursively();
    m_hls.insert(remoteUrl, job);

    QNetworkReply* reply = get(job.masterUrl);
    reply->setProperty("remoteUrl", remoteUrl);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleHlsMaster(reply); });
}

void TrailerCache::handleHlsMaster(QNetworkReply* reply)
{
    const QString remoteUrl = reply->property("remoteUrl").toString();
    reply->deleteLater();
    auto it = m_hls.find(remoteUrl);
    if (it == m_hls.end())
        return;
    if (reply->error() != QNetworkReply::NoError) {
        failHls(remoteUrl);
        return;
    }

    // One video variant (the tallest up to 720p, else the smallest) and the default audio.
    const QStringList lines = QString::fromUtf8(reply->readAll()).split(QLatin1Char('\n'));
    QString bestInf, bestUri, smallestInf, smallestUri, audioUri;
    int bestHeight = 0, smallestHeight = INT_MAX;
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines.at(i).trimmed();
        if (line.startsWith(QStringLiteral("#EXT-X-MEDIA:"))
            && attribute(line, QStringLiteral("TYPE")) == QStringLiteral("AUDIO")) {
            const QString uri = attribute(line, QStringLiteral("URI"));
            if (!uri.isEmpty() && (audioUri.isEmpty()
                                   || attribute(line, QStringLiteral("DEFAULT")) == QStringLiteral("YES")))
                audioUri = uri;
        } else if (line.startsWith(QStringLiteral("#EXT-X-STREAM-INF:"))) {
            QString uri;
            for (int j = i + 1; j < lines.size(); ++j) {
                const QString next = lines.at(j).trimmed();
                if (next.isEmpty() || next.startsWith(QLatin1Char('#')))
                    continue;
                uri = next;
                break;
            }
            if (uri.isEmpty())
                continue;
            const QString res = attribute(line, QStringLiteral("RESOLUTION"));
            const int height = res.section(QLatin1Char('x'), 1, 1).toInt();
            if (height <= kTargetHeight && height > bestHeight) {
                bestHeight = height;
                bestInf = line;
                bestUri = uri;
            }
            if (height < smallestHeight) {
                smallestHeight = height;
                smallestInf = line;
                smallestUri = uri;
            }
        }
    }
    if (bestUri.isEmpty()) {
        bestInf = smallestInf;
        bestUri = smallestUri;
    }
    if (bestUri.isEmpty()) {
        failHls(remoteUrl);
        return;
    }

    it->variantInf = bestInf;
    it->variantUri = bestUri;
    it->audioUri = audioUri;
    it->playlistsPending = audioUri.isEmpty() ? 1 : 2;

    const auto fetchPlaylist = [this, remoteUrl, master = it->masterUrl](const QString& uri,
                                                                        const QString& localName) {
        QNetworkReply* r = get(master.resolved(QUrl(uri)));
        r->setProperty("remoteUrl", remoteUrl);
        connect(r, &QNetworkReply::finished, this,
                [this, r, localName, uri]() { handleHlsPlaylist(r, localName, uri); });
    };
    fetchPlaylist(bestUri, QStringLiteral("video.m3u8"));
    if (!audioUri.isEmpty())
        fetchPlaylist(audioUri, QStringLiteral("audio.m3u8"));
}

void TrailerCache::handleHlsPlaylist(QNetworkReply* reply, const QString& localName,
                                     const QString& relUri)
{
    const QString remoteUrl = reply->property("remoteUrl").toString();
    reply->deleteLater();
    auto it = m_hls.find(remoteUrl);
    if (it == m_hls.end() || it->failed)
        return;
    if (reply->error() != QNetworkReply::NoError) {
        failHls(remoteUrl);
        return;
    }

    // Rewrite every URI to its path under the master's folder, and collect the files.
    const QUrl playlistUrl = it->masterUrl.resolved(QUrl(relUri));
    QStringList out;
    static const QRegularExpression mapUri(QStringLiteral("URI=\"([^\"]*)\""));
    for (const QString& raw : QString::fromUtf8(reply->readAll()).split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.startsWith(QStringLiteral("#EXT-X-MAP:"))) {
            const QRegularExpressionMatch m = mapUri.match(line);
            const QString rel = m.hasMatch() ? relativeToMaster(it->masterUrl, playlistUrl, m.captured(1))
                                             : QString();
            if (rel.isEmpty()) {
                failHls(remoteUrl);
                return;
            }
            it->files.append(rel);
            out.append(QString(line).replace(m.captured(1), rel));
        } else if (line.startsWith(QStringLiteral("#EXT-X-KEY"))) {
            failHls(remoteUrl);   // encrypted: not ours to handle
            return;
        } else if (!line.isEmpty() && !line.startsWith(QLatin1Char('#'))) {
            const QString rel = relativeToMaster(it->masterUrl, playlistUrl, line);
            if (rel.isEmpty()) {
                failHls(remoteUrl);
                return;
            }
            it->files.append(rel);
            out.append(rel);
        } else {
            out.append(line);
        }
    }

    QDir().mkpath(it->dir);
    QFile file(it->dir + QLatin1Char('/') + localName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(out.join(QLatin1Char('\n')).toUtf8()) < 0) {
        failHls(remoteUrl);
        return;
    }
    file.close();

    if (--it->playlistsPending == 0) {
        it->files.removeDuplicates();
        pumpHls(remoteUrl);
    }
}

void TrailerCache::pumpHls(const QString& remoteUrl)
{
    auto it = m_hls.find(remoteUrl);
    if (it == m_hls.end() || it->failed)
        return;
    if (it->done == it->files.size()) {
        finishHls(remoteUrl);
        return;
    }
    while (it->running < kHlsParallel && it->next < it->files.size()) {
        const QString rel = it->files.at(it->next++);
        ++it->running;
        QUrl url = it->masterUrl.resolved(QUrl(rel));
        QNetworkReply* reply = get(url);
        reply->setProperty("remoteUrl", remoteUrl);
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, rel]() { handleHlsFile(reply, rel); });
    }
}

void TrailerCache::handleHlsFile(QNetworkReply* reply, const QString& relPath)
{
    const QString remoteUrl = reply->property("remoteUrl").toString();
    reply->deleteLater();
    auto it = m_hls.find(remoteUrl);
    if (it == m_hls.end() || it->failed)
        return;
    --it->running;
    const bool ok = reply->error() == QNetworkReply::NoError
        && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() < 400;
    const QByteArray payload = ok ? reply->readAll() : QByteArray();
    it->bytes += payload.size();
    if (payload.isEmpty() || it->bytes > kMaxHlsBytes) {
        failHls(remoteUrl);
        return;
    }

    const QString path = it->dir + QLatin1Char('/') + relPath;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(payload) != payload.size()) {
        failHls(remoteUrl);
        return;
    }
    file.close();

    ++it->done;
    emit progress(remoteUrl, double(it->done) / double(qMax(1, int(it->files.size()))));
    pumpHls(remoteUrl);
}

void TrailerCache::finishHls(const QString& remoteUrl)
{
    const HlsJob job = m_hls.take(remoteUrl);
    m_inFlight.remove(remoteUrl);

    QStringList master{QStringLiteral("#EXTM3U"), QStringLiteral("#EXT-X-VERSION:7"),
                       QStringLiteral("#EXT-X-INDEPENDENT-SEGMENTS")};
    QString inf = job.variantInf;
    if (!job.audioUri.isEmpty()) {
        master.append(QStringLiteral("#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio\",NAME=\"Default\","
                                     "AUTOSELECT=YES,DEFAULT=YES,URI=\"audio.m3u8\""));
        static const QRegularExpression audioGroup(QStringLiteral("AUDIO=\"[^\"]*\""));
        inf.replace(audioGroup, QStringLiteral("AUDIO=\"audio\""));
    }
    master.append(inf);
    master.append(QStringLiteral("video.m3u8"));

    const QString path = job.dir + QStringLiteral("/master.m3u8");
    const QString tmp = path + QStringLiteral(".part");
    QFile file(tmp);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(master.join(QLatin1Char('\n')).toUtf8() + '\n') < 0) {
        QDir(job.dir).removeRecursively();
        emit failed(remoteUrl);
        return;
    }
    file.close();
    QFile::remove(path);
    if (!QFile::rename(tmp, path)) {
        QDir(job.dir).removeRecursively();
        emit failed(remoteUrl);
        return;
    }
    trimToBudget();
    emit ready(remoteUrl, QUrl::fromLocalFile(path).toString());
}

void TrailerCache::failHls(const QString& remoteUrl)
{
    auto it = m_hls.find(remoteUrl);
    if (it == m_hls.end() || it->failed)
        return;
    it->failed = true;
    const QString dir = it->dir;
    m_hls.erase(it);   // replies still running find no job and drop their data
    m_inFlight.remove(remoteUrl);
    QDir(dir).removeRecursively();
    emit failed(remoteUrl);
}

void TrailerCache::trimToBudget()
{
    const QFileInfoList entries =
        QDir(cacheDir()).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    QList<qint64> sizes;
    qint64 total = 0;
    for (const QFileInfo& f : entries) {
        sizes.append(entrySize(f));
        total += sizes.last();
    }
    // Newest first: drop from the old end until it fits (the newest trailer always stays).
    for (int i = static_cast<int>(entries.size()) - 1; i > 0 && total > kCacheBudgetBytes; --i) {
        const QFileInfo& f = entries.at(i);
        if (f.isDir()) {
            bool busy = false;
            for (const HlsJob& job : std::as_const(m_hls))
                busy = busy || job.dir == f.absoluteFilePath();
            if (busy)
                continue;
            QDir(f.absoluteFilePath()).removeRecursively();
        } else {
            QFile::remove(f.absoluteFilePath());
        }
        total -= sizes.at(i);
    }
}

} // namespace arachnel::core

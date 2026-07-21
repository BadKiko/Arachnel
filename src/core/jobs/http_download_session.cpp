#include "http_download_session.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

namespace arachnel::core {

struct HttpDownloadSession::Impl {
    QNetworkAccessManager network;
    QHash<QString, QNetworkReply*> replies;
};

namespace {

QString filenameFromContentDisposition(const QString& header)
{
    const QRegularExpression re(QStringLiteral(R"(filename\*?=(?:UTF-8''|")?([^";]+))"),
                              QRegularExpression::CaseInsensitiveOption);
    const auto match = re.match(header);
    if (!match.hasMatch())
        return {};
    return QUrl::fromPercentEncoding(match.captured(1).toUtf8());
}

QString sanitizeFileName(QString name)
{
    name.replace(QRegularExpression(QStringLiteral(R"([<>:"/\\|?*])")), QStringLiteral("_"));
    return name.trimmed();
}

} // namespace

HttpDownloadSession::HttpDownloadSession(QObject* parent)
    : QObject(parent)
    , m_impl(new Impl)
{
}

HttpDownloadSession::~HttpDownloadSession()
{
    shutdown();
}

void HttpDownloadSession::shutdown()
{
    if (!m_impl)
        return;

    for (auto it = m_impl->replies.begin(); it != m_impl->replies.end(); ++it) {
        QNetworkReply* reply = it.value();
        if (!reply)
            continue;
        reply->disconnect(this);
        reply->abort();
        delete reply;
    }
    m_impl->replies.clear();
    delete m_impl;
    m_impl = nullptr;
}

bool HttpDownloadSession::addJob(const QString& jobId, const QString& url, const QString& referer,
                                 const QString& saveDirectory)
{
    if (!m_impl)
        return false;
    if (jobId.isEmpty() || url.isEmpty() || saveDirectory.isEmpty())
        return false;
    if (m_impl->replies.contains(jobId))
        return false;

    if (!QDir().mkpath(saveDirectory))
        return false;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) Arachnel/0.1"));
    if (!referer.isEmpty())
        request.setRawHeader("Referer", referer.toUtf8());

    QNetworkReply* reply = m_impl->network.get(request);
    m_impl->replies.insert(jobId, reply);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, jobId](qint64 received, qint64 total) {
                const int progress =
                    total > 0 ? static_cast<int>((received * 100) / total) : 0;
                emit httpProgress(jobId, progress, received, total);
            });

    connect(reply, &QNetworkReply::finished, this, [this, jobId, reply, saveDirectory]() {
        if (!m_impl) {
            delete reply;
            return;
        }
        m_impl->replies.remove(jobId);

        if (reply->error() != QNetworkReply::NoError) {
            const QString error = reply->errorString();
            reply->deleteLater();
            emit httpFailed(jobId, error);
            return;
        }

        QString fileName = filenameFromContentDisposition(
            reply->header(QNetworkRequest::ContentDispositionHeader).toString());
        if (fileName.isEmpty()) {
            const QUrl sourceUrl(reply->url());
            fileName = QFileInfo(sourceUrl.path()).fileName();
        }
        fileName = sanitizeFileName(fileName);
        if (fileName.isEmpty())
            fileName = QStringLiteral("download.bin");

        const QString filePath = QDir(saveDirectory).absoluteFilePath(fileName);
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            reply->deleteLater();
            emit httpFailed(jobId, QStringLiteral("Не удалось сохранить файл"));
            return;
        }
        file.write(reply->readAll());
        file.close();
        reply->deleteLater();
        emit httpFinished(jobId, filePath);
    });

    return true;
}

void HttpDownloadSession::cancel(const QString& jobId)
{
    if (!m_impl)
        return;
    if (QNetworkReply* reply = m_impl->replies.take(jobId)) {
        reply->abort();
        delete reply;
    }
}

} // namespace arachnel::core

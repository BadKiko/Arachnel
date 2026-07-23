#include "plugin_catalog_service.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QUrl>
#include <QVariantMap>

namespace arachnel::core {

namespace {

const char* kDefaultCatalogUrl =
    "https://gitlab.com/BadKiko/arachnel-plugins-sourcelist/-/raw/main/plugins.json";

} // namespace

PluginCatalogService::PluginCatalogService(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

QString PluginCatalogService::catalogUrl() const
{
    return QString::fromUtf8(kDefaultCatalogUrl);
}

QString PluginCatalogService::currentPlatformId() const
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("linux");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#else
    return QStringLiteral("unknown");
#endif
}

void PluginCatalogService::setLoading(bool value)
{
    if (m_loading == value)
        return;
    m_loading = value;
    emit loadingChanged();
}

void PluginCatalogService::setInstalling(const QString& pluginId)
{
    const bool next = !pluginId.isEmpty();
    if (m_installing == next && m_installingPluginId == pluginId)
        return;
    m_installing = next;
    m_installingPluginId = pluginId;
    if (next)
        setDownloadProgress(0);
    emit installingChanged();
}

void PluginCatalogService::setDownloadProgress(int percent)
{
    const int clamped = qBound(0, percent, 100);
    if (m_downloadProgress == clamped)
        return;
    m_downloadProgress = clamped;
    emit downloadProgressChanged();
}

void PluginCatalogService::setError(const QString& message)
{
    if (m_error == message)
        return;
    m_error = message;
    emit errorChanged();
}

void PluginCatalogService::refresh()
{
    if (m_catalogReply) {
        m_catalogReply->abort();
        m_catalogReply->deleteLater();
        m_catalogReply = nullptr;
    }

    setError({});
    setLoading(true);

    QNetworkRequest request{QUrl(catalogUrl())};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Arachnel/%1").arg(QCoreApplication::applicationVersion()));

    m_catalogReply = m_network->get(request);
    connect(m_catalogReply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply* reply = m_catalogReply;
        m_catalogReply = nullptr;
        setLoading(false);
        if (!reply)
            return;

        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            setError(QCoreApplication::translate("Core", "Could not load plugin list: %1")
                         .arg(reply->errorString()));
            m_plugins.clear();
            emit pluginsChanged();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            setError(QCoreApplication::translate("Core", "Plugin list is invalid"));
            m_plugins.clear();
            emit pluginsChanged();
            return;
        }

        const QJsonArray arr = doc.object().value(QStringLiteral("plugins")).toArray();
        QVariantList next;
        next.reserve(arr.size());
        const QString platform = currentPlatformId();
        for (const QJsonValue& value : arr) {
            if (!value.isObject())
                continue;
            const QJsonObject obj = value.toObject();
            QVariantMap row;
            row.insert(QStringLiteral("id"), obj.value(QStringLiteral("id")).toString());
            row.insert(QStringLiteral("name"), obj.value(QStringLiteral("name")).toString());
            row.insert(QStringLiteral("description"),
                       obj.value(QStringLiteral("description")).toString());
            row.insert(QStringLiteral("version"), obj.value(QStringLiteral("version")).toString());
            row.insert(QStringLiteral("apiVersion"),
                       obj.value(QStringLiteral("apiVersion")).toInt(0));
            row.insert(QStringLiteral("iconName"),
                       obj.value(QStringLiteral("iconName")).toString(QStringLiteral("extension")));
            row.insert(QStringLiteral("url"), obj.value(QStringLiteral("url")).toString());
            row.insert(QStringLiteral("sha256"), obj.value(QStringLiteral("sha256")).toString());
            row.insert(QStringLiteral("size"), obj.value(QStringLiteral("size")).toVariant());

            QStringList platforms;
            const QJsonArray plats = obj.value(QStringLiteral("platforms")).toArray();
            for (const QJsonValue& p : plats)
                platforms.append(p.toString());
            row.insert(QStringLiteral("platforms"), platforms);

            const bool supported =
                platforms.isEmpty() || platforms.contains(platform) || platforms.contains(QStringLiteral("all"));
            row.insert(QStringLiteral("supported"), supported);
            if (row.value(QStringLiteral("id")).toString().isEmpty())
                continue;
            if (!supported)
                continue;
            next.append(row);
        }

        m_plugins = next;
        emit pluginsChanged();
    });
}

QString PluginCatalogService::downloadUrlForEntry(const QVariantMap& entry) const
{
    return entry.value(QStringLiteral("url")).toString().trimmed();
}

void PluginCatalogService::installPlugin(const QString& pluginId)
{
    const QString id = pluginId.trimmed();
    if (id.isEmpty())
        return;

    if (id != m_installingPluginId && !m_installQueue.contains(id))
        m_installQueue.append(id);

    if (!m_installing)
        processInstallQueue();
}

bool PluginCatalogService::beginInstall(const QString& pluginId)
{
    QVariantMap found;
    for (const QVariant& item : m_plugins) {
        const QVariantMap row = item.toMap();
        if (row.value(QStringLiteral("id")).toString() == pluginId) {
            found = row;
            break;
        }
    }
    if (found.isEmpty()) {
        setError(QCoreApplication::translate("Core", "Plugin not found in the official list"));
        emit installFinished(pluginId, false, m_error);
        return false;
    }

    const QString url = downloadUrlForEntry(found);
    if (url.isEmpty()) {
        setError(QCoreApplication::translate("Core", "No download link for this plugin"));
        emit installFinished(pluginId, false, m_error);
        return false;
    }

    m_pendingInstallId = pluginId;
    setError({});
    setInstalling(pluginId);
    downloadAndInstall(found);
    return true;
}

void PluginCatalogService::finishInstallAttempt(const QString& pluginId, bool ok,
                                                const QString& pathOrError)
{
    setInstalling({});
    emit installFinished(pluginId, ok, pathOrError);
    processInstallQueue();
}

void PluginCatalogService::processInstallQueue()
{
    while (!m_installQueue.isEmpty()) {
        const QString next = m_installQueue.takeFirst();
        if (beginInstall(next))
            return;
    }
    emit installQueueDrained();
}

void PluginCatalogService::downloadAndInstall(const QVariantMap& entry)
{
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }

    const QString url = downloadUrlForEntry(entry);
    const QString expectedSha = entry.value(QStringLiteral("sha256")).toString().trimmed().toLower();
    const QString pluginId = entry.value(QStringLiteral("id")).toString();

    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/arachnel-plugins");
    QDir().mkpath(dir);
    const QString path = dir + QLatin1Char('/') + pluginId + QStringLiteral(".arach");
    QFile::remove(path);

    auto* outFile = new QFile(path, this);
    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString err = QCoreApplication::translate("Core", "Could not save plugin file");
        setError(err);
        outFile->deleteLater();
        finishInstallAttempt(pluginId, false, err);
        return;
    }

    auto* hasher = new QCryptographicHash(QCryptographicHash::Sha256);
    setDownloadProgress(0);

    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Arachnel/%1").arg(QCoreApplication::applicationVersion()));

    m_downloadReply = m_network->get(request);
    connect(m_downloadReply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                if (total > 0)
                    setDownloadProgress(static_cast<int>((received * 100) / total));
            });
    connect(m_downloadReply, &QNetworkReply::readyRead, this, [this, outFile, hasher]() {
        if (!m_downloadReply || !outFile)
            return;
        const QByteArray chunk = m_downloadReply->readAll();
        if (chunk.isEmpty())
            return;
        hasher->addData(chunk);
        outFile->write(chunk);
    });
    connect(m_downloadReply, &QNetworkReply::finished, this,
            [this, expectedSha, pluginId, outFile, hasher, path]() {
                QNetworkReply* reply = m_downloadReply;
                m_downloadReply = nullptr;
                const auto cleanup = qScopeGuard([outFile, hasher, reply]() {
                    if (outFile) {
                        outFile->close();
                        outFile->deleteLater();
                    }
                    delete hasher;
                    if (reply)
                        reply->deleteLater();
                });

                if (!reply) {
                    QFile::remove(path);
                    finishInstallAttempt(pluginId, false,
                                         QCoreApplication::translate("Core", "Download failed"));
                    return;
                }

                // Flush any remaining buffered bytes.
                const QByteArray rest = reply->readAll();
                if (!rest.isEmpty()) {
                    hasher->addData(rest);
                    outFile->write(rest);
                }
                outFile->close();

                if (reply->error() != QNetworkReply::NoError) {
                    QFile::remove(path);
                    const QString err =
                        QCoreApplication::translate("Core", "Download failed: %1")
                            .arg(reply->errorString());
                    setError(err);
                    finishInstallAttempt(pluginId, false, err);
                    return;
                }

                if (outFile->size() <= 0) {
                    QFile::remove(path);
                    const QString err =
                        QCoreApplication::translate("Core", "Downloaded plugin file is empty");
                    setError(err);
                    finishInstallAttempt(pluginId, false, err);
                    return;
                }

                if (!expectedSha.isEmpty()) {
                    const QString actual =
                        QString::fromLatin1(hasher->result().toHex()).toLower();
                    if (actual != expectedSha) {
                        QFile::remove(path);
                        const QString err = QCoreApplication::translate(
                            "Core", "Plugin file checksum mismatch");
                        setError(err);
                        finishInstallAttempt(pluginId, false, err);
                        return;
                    }
                }

                setDownloadProgress(100);
                finishInstallAttempt(pluginId, true, path);
            });
}
} // namespace arachnel::core

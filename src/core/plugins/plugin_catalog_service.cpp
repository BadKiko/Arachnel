#include "plugin_catalog_service.h"

#include "plugin_api.h"
#include "plugin_urls.h"
#include "plugin_version.h"

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

#include <algorithm>

namespace arachnel::core {

namespace {

const char* kDefaultCatalogUrl =
    "https://gitlab.com/BadKiko/arachnel-plugins-sourcelist/-/raw/main/plugins.json";

QStringList platformsFromJson(const QJsonValue& value)
{
    QStringList platforms;
    if (value.isArray()) {
        const QJsonArray plats = value.toArray();
        for (const QJsonValue& p : plats)
            platforms.append(p.toString());
    }
    return platforms;
}

bool platformsSupported(const QStringList& platforms, const QString& platform)
{
    return platforms.isEmpty() || platforms.contains(platform)
           || platforms.contains(QStringLiteral("all"));
}

/** Pick newest build compatible with this Arachnel + platform. */
QJsonObject pickCompatibleBuild(const QJsonObject& pluginObj, const QString& appVersion,
                                const QString& platform)
{
    const QJsonArray builds = pluginObj.value(QStringLiteral("builds")).toArray();
    if (builds.isEmpty()) {
        // Schema v1 flat entry.
        QJsonObject flat;
        flat.insert(QStringLiteral("version"), pluginObj.value(QStringLiteral("version")));
        flat.insert(QStringLiteral("apiVersion"), pluginObj.value(QStringLiteral("apiVersion")));
        flat.insert(QStringLiteral("minArachnel"),
                    pluginObj.value(QStringLiteral("minArachnel")).toString(QStringLiteral("0.0.0")));
        flat.insert(QStringLiteral("maxArachnel"),
                    pluginObj.value(QStringLiteral("maxArachnel")).toString());
        flat.insert(QStringLiteral("url"), pluginObj.value(QStringLiteral("url")));
        flat.insert(QStringLiteral("sha256"), pluginObj.value(QStringLiteral("sha256")));
        flat.insert(QStringLiteral("size"), pluginObj.value(QStringLiteral("size")));
        flat.insert(QStringLiteral("platforms"), pluginObj.value(QStringLiteral("platforms")));
        flat.insert(QStringLiteral("abiToken"), pluginObj.value(QStringLiteral("abiToken")));
        flat.insert(QStringLiteral("file"), pluginObj.value(QStringLiteral("file")));
        if (!platformsSupported(platformsFromJson(flat.value(QStringLiteral("platforms"))),
                                platform))
            return {};
        if (!appVersionInRange(appVersion,
                               flat.value(QStringLiteral("minArachnel")).toString(),
                               flat.value(QStringLiteral("maxArachnel")).toString()))
            return {};
        const int api = flat.value(QStringLiteral("apiVersion")).toInt(0);
        if (api > ARACHNEL_PLUGIN_API_VERSION || api < ARACHNEL_PLUGIN_API_VERSION_MIN)
            return {};
        return flat;
    }

    QJsonObject best;
    QString bestVersion;
    for (const QJsonValue& value : builds) {
        if (!value.isObject())
            continue;
        const QJsonObject build = value.toObject();
        if (!platformsSupported(platformsFromJson(build.value(QStringLiteral("platforms"))),
                                platform))
            continue;
        if (!appVersionInRange(appVersion,
                               build.value(QStringLiteral("minArachnel"))
                                   .toString(QStringLiteral("0.0.0")),
                               build.value(QStringLiteral("maxArachnel")).toString()))
            continue;
        const int api = build.value(QStringLiteral("apiVersion")).toInt(0);
        if (api > ARACHNEL_PLUGIN_API_VERSION || api < ARACHNEL_PLUGIN_API_VERSION_MIN)
            continue;
        const QString ver = build.value(QStringLiteral("version")).toString();
        if (best.isEmpty() || comparePluginVersions(ver, bestVersion) > 0) {
            best = build;
            bestVersion = ver;
        }
    }
    return best;
}

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
        // Capture-by-reply below; disconnect so an aborted reply cannot steal the new one.
        QObject::disconnect(m_catalogReply, nullptr, this, nullptr);
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

    QNetworkReply* reply = m_network->get(request);
    m_catalogReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_catalogReply == reply)
            m_catalogReply = nullptr;
        setLoading(false);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            // Superseded/aborted refresh — keep current list, don't wipe UI.
            if (reply->error() == QNetworkReply::OperationCanceledError)
                return;
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

        const QJsonObject root = doc.object();
        const QJsonArray arr = root.value(QStringLiteral("plugins")).toArray();
        QVariantList next;
        next.reserve(arr.size());
        const QString platform = currentPlatformId();
        const QString appVersion = QCoreApplication::applicationVersion();
        for (const QJsonValue& value : arr) {
            if (!value.isObject())
                continue;
            const QJsonObject obj = value.toObject();
            const QString pluginId = obj.value(QStringLiteral("id")).toString().trimmed();
            if (pluginId.isEmpty())
                continue;

            const QJsonObject build = pickCompatibleBuild(obj, appVersion, platform);
            if (build.isEmpty())
                continue;

            QVariantMap row;
            row.insert(QStringLiteral("id"), pluginId);
            row.insert(QStringLiteral("name"), obj.value(QStringLiteral("name")).toString());
            row.insert(QStringLiteral("description"),
                       obj.value(QStringLiteral("description")).toString());
            row.insert(QStringLiteral("version"),
                       build.value(QStringLiteral("version")).toString());
            row.insert(QStringLiteral("apiVersion"),
                       build.value(QStringLiteral("apiVersion")).toInt(0));
            row.insert(QStringLiteral("iconName"),
                       obj.value(QStringLiteral("iconName")).toString(QStringLiteral("extension")));
            row.insert(QStringLiteral("url"), build.value(QStringLiteral("url")).toString());
            row.insert(QStringLiteral("sha256"), build.value(QStringLiteral("sha256")).toString());
            row.insert(QStringLiteral("size"), build.value(QStringLiteral("size")).toVariant());
            row.insert(QStringLiteral("abiToken"),
                       build.value(QStringLiteral("abiToken")).toString());
            row.insert(QStringLiteral("minArachnel"),
                       build.value(QStringLiteral("minArachnel")).toString());
            row.insert(QStringLiteral("maxArachnel"),
                       build.value(QStringLiteral("maxArachnel")).toString());
            row.insert(QStringLiteral("repository"), resolvePluginRepository(pluginId, obj));
            row.insert(QStringLiteral("recommended"),
                       obj.value(QStringLiteral("recommended")).toBool(false)
                           || pluginId == QStringLiteral("steamidra"));

            const QStringList platforms =
                platformsFromJson(build.value(QStringLiteral("platforms")));
            row.insert(QStringLiteral("platforms"), platforms);
            row.insert(QStringLiteral("supported"), true);
            next.append(row);
        }

        std::sort(next.begin(), next.end(), [](const QVariant& a, const QVariant& b) {
            const QVariantMap left = a.toMap();
            const QVariantMap right = b.toMap();
            const bool leftRec = left.value(QStringLiteral("recommended")).toBool();
            const bool rightRec = right.value(QStringLiteral("recommended")).toBool();
            if (leftRec != rightRec)
                return leftRec;
            const QString leftId = left.value(QStringLiteral("id")).toString();
            const QString rightId = right.value(QStringLiteral("id")).toString();
            if (leftId == QStringLiteral("steamidra") && rightId != QStringLiteral("steamidra"))
                return true;
            if (rightId == QStringLiteral("steamidra") && leftId != QStringLiteral("steamidra"))
                return false;
            return QString::localeAwareCompare(left.value(QStringLiteral("name")).toString(),
                                               right.value(QStringLiteral("name")).toString())
                   < 0;
        });

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
        QObject::disconnect(m_downloadReply, nullptr, this, nullptr);
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

    QNetworkReply* reply = m_network->get(request);
    m_downloadReply = reply;
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, reply](qint64 received, qint64 total) {
                if (m_downloadReply != reply)
                    return;
                if (total > 0)
                    setDownloadProgress(static_cast<int>((received * 100) / total));
            });
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, outFile, hasher]() {
        if (m_downloadReply != reply || !outFile)
            return;
        const QByteArray chunk = reply->readAll();
        if (chunk.isEmpty())
            return;
        hasher->addData(chunk);
        outFile->write(chunk);
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, expectedSha, pluginId, outFile, hasher, path]() {
                if (m_downloadReply == reply)
                    m_downloadReply = nullptr;
                const auto cleanup = qScopeGuard([outFile, hasher, reply]() {
                    if (outFile) {
                        outFile->close();
                        outFile->deleteLater();
                    }
                    delete hasher;
                    reply->deleteLater();
                });

                // Flush any remaining buffered bytes.
                const QByteArray rest = reply->readAll();
                if (!rest.isEmpty()) {
                    hasher->addData(rest);
                    outFile->write(rest);
                }
                outFile->close();

                if (reply->error() != QNetworkReply::NoError) {
                    QFile::remove(path);
                    if (reply->error() == QNetworkReply::OperationCanceledError) {
                        finishInstallAttempt(pluginId, false,
                                             QCoreApplication::translate("Core", "Download failed"));
                        return;
                    }
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

#include "freetp_plugin.h"

#include "archive_installer.h"

#include "catalog_parser.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>

namespace freetp {

namespace {

constexpr auto kSourceId = "freetp";
constexpr auto kDefaultCatalogUrl =
    "https://gitlab.com/BadKiko/freetp-hydra-link/-/raw/main/games.json?ref_type=heads";

bool isPlaceholderCatalog(const QByteArray& payload)
{
    const auto entries =
        arachnel::core::parseCatalogFeed(payload, QString::fromLatin1(kSourceId));
    if (entries.size() != 1)
        return false;
    return entries.first().id == QStringLiteral("freetp-example-game");
}

QByteArray downloadCatalogBytes()
{
    QNetworkAccessManager network;
    QNetworkRequest request(QUrl(QString::fromLatin1(kDefaultCatalogUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
    request.setTransferTimeout(60000);

    QNetworkReply* reply = network.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QByteArray payload;
    if (reply->error() == QNetworkReply::NoError)
        payload = reply->readAll();
    reply->deleteLater();
    return payload;
}

QByteArray readCatalogBytes(const QString& rootPath, bool preferRemote)
{
    const QString localPath = rootPath + QStringLiteral("/games.json");

    QByteArray local;
    QFile localFile(localPath);
    if (localFile.open(QIODevice::ReadOnly))
        local = localFile.readAll();

    const bool localUsable = !local.isEmpty() && !isPlaceholderCatalog(local);
    if (!preferRemote && localUsable)
        return local;

    const QByteArray remote = downloadCatalogBytes();
    if (!remote.isEmpty() && !isPlaceholderCatalog(remote)) {
        QFile cache(localPath);
        if (cache.open(QIODevice::WriteOnly))
            cache.write(remote);
        return remote;
    }

    if (!local.isEmpty())
        return local;
    return remote;
}

} // namespace

FreetpPlugin::FreetpPlugin(QString rootPath)
    : m_rootPath(std::move(rootPath))
{
}

QString FreetpPlugin::id() const
{
    return QString::fromLatin1(kSourceId);
}

QString FreetpPlugin::name() const
{
    return QStringLiteral("FreeTP");
}

QString FreetpPlugin::description() const
{
    return QStringLiteral("Торрент-каталог FreeTP — magnet-ссылки, portable-установка");
}

QString FreetpPlugin::version() const
{
    return QStringLiteral("1.0.0");
}

QStringList FreetpPlugin::capabilities() const
{
    return {QStringLiteral("search"), QStringLiteral("install"), QStringLiteral("update"),
            QStringLiteral("launch")};
}

void FreetpPlugin::resetCatalogCache()
{
    m_catalogLoaded = false;
    m_catalog.clear();
    m_forceRemoteCatalog = true;
}

void FreetpPlugin::ensureCatalogLoaded() const
{
    if (m_catalogLoaded)
        return;
    m_catalogLoaded = true;

    const QByteArray payload = readCatalogBytes(m_rootPath, m_forceRemoteCatalog);
    m_forceRemoteCatalog = false;
    if (payload.isEmpty())
        return;

    m_catalog = arachnel::core::parseCatalogFeed(payload, id());
    for (auto& entry : m_catalog)
        entry.installKind = arachnel::core::InstallKind::PortableArchive;
}

QVector<arachnel::core::CatalogEntry> FreetpPlugin::catalog() const
{
    ensureCatalogLoaded();
    return m_catalog;
}

QVector<arachnel::core::CatalogEntry> FreetpPlugin::search(const QString& query) const
{
    ensureCatalogLoaded();
    const QString needle = query.trimmed().toLower();
    if (needle.isEmpty())
        return m_catalog;

    QVector<arachnel::core::CatalogEntry> filtered;
    filtered.reserve(m_catalog.size());
    for (const auto& entry : m_catalog) {
        if (entry.title.toLower().contains(needle))
            filtered.append(entry);
    }
    return filtered;
}

std::optional<arachnel::core::CatalogEntry> FreetpPlugin::entryById(
    const QString& entryId) const
{
    ensureCatalogLoaded();
    for (const auto& entry : m_catalog) {
        if (entry.id == entryId)
            return entry;
    }
    return std::nullopt;
}

arachnel::core::InstallResult FreetpPlugin::installFromDownload(
    const arachnel::core::InstallContext& ctx) const
{
    arachnel::core::InstallResult result;

    const QString contentRoot = findDownloadContentRoot(ctx.downloadPath);
    if (contentRoot.isEmpty() || !QDir(contentRoot).exists()) {
        result.success = false;
        result.error = QStringLiteral("Файлы загрузки не найдены");
        return result;
    }

    QString error;
    QString installPath = installPortableFromDownload(contentRoot, ctx.targetPath, &error);

    if (installPath.isEmpty()) {
        // Stub: пока без полного пайплайна — берём папку загрузки как installPath
        const QString exe = findGameExecutable(contentRoot);
        installPath = exe.isEmpty() ? contentRoot : QFileInfo(exe).absolutePath();
    }

    result.success = true;
    result.installPath = installPath;
    return result;
}

std::optional<QString> FreetpPlugin::detectUpdate(const arachnel::core::LibraryGame& local,
                                                  const arachnel::core::CatalogEntry& remote) const
{
    if (remote.uploadDate.isEmpty() || local.uploadDate.isEmpty())
        return std::nullopt;

    const QDateTime remoteDate = QDateTime::fromString(remote.uploadDate, Qt::ISODate);
    const QDateTime localDate = QDateTime::fromString(local.uploadDate, Qt::ISODate);
    if (remoteDate.isValid() && localDate.isValid()) {
        if (remoteDate > localDate)
            return remote.uploadDate;
        return std::nullopt;
    }

    if (remote.uploadDate > local.uploadDate)
        return remote.uploadDate;
    return std::nullopt;
}

arachnel::core::LaunchInfo FreetpPlugin::launchInfo(const arachnel::core::LibraryGame& local) const
{
    arachnel::core::LaunchInfo info;
    if (local.installPath.isEmpty())
        return info;

    const QString exe = findGameExecutable(local.installPath);
    if (exe.isEmpty())
        return info;

    info.executable = exe;
    info.workingDirectory = QFileInfo(exe).absolutePath();
    return info;
}

} // namespace freetp

#include "settings_store.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace arachnel::core {

namespace {

QString settingsFilePath()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/settings.json");
}

QString defaultLibraryRoot()
{
    return QDir::homePath() + QStringLiteral("/Games/Arachnel");
}

QString defaultDownloadsRoot()
{
    return QDir::homePath() + QStringLiteral("/Downloads/Arachnel");
}

QString defaultFreetpCatalogUrl()
{
    return QStringLiteral(
        "https://gitlab.com/BadKiko/freetp-hydra-link/-/raw/main/games.json?ref_type=heads");
}

} // namespace

SettingsStore::SettingsStore(QObject* parent)
    : QObject(parent)
    , m_libraryRoot(defaultLibraryRoot())
    , m_downloadsRoot(defaultDownloadsRoot())
    , m_freetpCatalogUrl(defaultFreetpCatalogUrl())
{
}

QString SettingsStore::catalogUrlForSource(const QString& sourceId) const
{
    if (sourceId == QStringLiteral("freetp"))
        return m_freetpCatalogUrl;
    return {};
}

QString SettingsStore::resolvedLibraryRoot() const
{
    return m_libraryRoot.isEmpty() ? defaultLibraryRoot() : m_libraryRoot;
}

QString SettingsStore::resolvedDownloadsRoot() const
{
    return m_downloadsRoot.isEmpty() ? defaultDownloadsRoot() : m_downloadsRoot;
}

void SettingsStore::setLibraryRoot(const QString& path)
{
    if (m_libraryRoot == path)
        return;
    m_libraryRoot = path;
    emit libraryRootChanged();
    save();
}

void SettingsStore::setDownloadsRoot(const QString& path)
{
    if (m_downloadsRoot == path)
        return;
    m_downloadsRoot = path;
    emit downloadsRootChanged();
    save();
}

void SettingsStore::setFreetpCatalogUrl(const QString& url)
{
    if (m_freetpCatalogUrl == url)
        return;
    m_freetpCatalogUrl = url;
    emit freetpCatalogUrlChanged();
    save();
}

void SettingsStore::load()
{
    QFile file(settingsFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    if (obj.contains(QStringLiteral("libraryRoot")))
        m_libraryRoot = obj.value(QStringLiteral("libraryRoot")).toString(m_libraryRoot);
    if (obj.contains(QStringLiteral("downloadsRoot")))
        m_downloadsRoot = obj.value(QStringLiteral("downloadsRoot")).toString(m_downloadsRoot);
    if (obj.contains(QStringLiteral("freetpCatalogUrl")))
        m_freetpCatalogUrl =
            obj.value(QStringLiteral("freetpCatalogUrl")).toString(m_freetpCatalogUrl);

    emit libraryRootChanged();
    emit downloadsRootChanged();
    emit freetpCatalogUrlChanged();
}

void SettingsStore::save()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("libraryRoot"), m_libraryRoot);
    obj.insert(QStringLiteral("downloadsRoot"), m_downloadsRoot);
    obj.insert(QStringLiteral("freetpCatalogUrl"), m_freetpCatalogUrl);

    QFile file(settingsFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

} // namespace arachnel::core

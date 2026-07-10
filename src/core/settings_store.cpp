#include "settings_store.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace arachnel::core {

namespace {

QString settingsFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
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

SourcePluginInfo sourceFromJson(const QJsonObject& obj)
{
    SourcePluginInfo info;
    info.id = obj.value(QStringLiteral("id")).toString();
    info.name = obj.value(QStringLiteral("name")).toString();
    info.description = obj.value(QStringLiteral("description")).toString();
    info.catalogUrl = obj.value(QStringLiteral("catalogUrl")).toString();
    info.iconName = obj.value(QStringLiteral("iconName")).toString(QStringLiteral("storefront"));
    info.enabled = obj.value(QStringLiteral("enabled")).toBool(true);

    const QJsonArray caps = obj.value(QStringLiteral("capabilities")).toArray();
    for (const QJsonValue& cap : caps)
        info.capabilities.append(cap.toString());
    if (info.capabilities.isEmpty()) {
        info.capabilities = {QStringLiteral("search"), QStringLiteral("install"),
                             QStringLiteral("update")};
    }
    return info;
}

QJsonObject sourceToJson(const SourcePluginInfo& info)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), info.id);
    obj.insert(QStringLiteral("name"), info.name);
    obj.insert(QStringLiteral("description"), info.description);
    obj.insert(QStringLiteral("catalogUrl"), info.catalogUrl);
    obj.insert(QStringLiteral("iconName"), info.iconName);
    obj.insert(QStringLiteral("enabled"), info.enabled);

    QJsonArray caps;
    for (const QString& cap : info.capabilities)
        caps.append(cap);
    obj.insert(QStringLiteral("capabilities"), caps);
    return obj;
}

} // namespace

SettingsStore::SettingsStore(QObject* parent)
    : QObject(parent)
    , m_libraryRoot(defaultLibraryRoot())
    , m_downloadsRoot(defaultDownloadsRoot())
    , m_maxConcurrentDownloads(2)
    , m_sources(defaultSources())
{
}

void SettingsStore::setSources(QVector<SourcePluginInfo> sources)
{
    m_sources = std::move(sources);
    emit sourcesChanged();
    save();
}

void SettingsStore::persistSources(QVector<SourcePluginInfo> sources)
{
    m_sources = std::move(sources);
    save();
}

QString SettingsStore::catalogUrlForSource(const QString& sourceId) const
{
    for (const auto& source : m_sources) {
        if (source.id == sourceId)
            return source.catalogUrl;
    }
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

void SettingsStore::setMaxConcurrentDownloads(int count)
{
    const int clamped = qBound(1, count, 8);
    if (m_maxConcurrentDownloads == clamped)
        return;
    m_maxConcurrentDownloads = clamped;
    emit maxConcurrentDownloadsChanged();
    save();
}

void SettingsStore::load()
{
    QFile file(settingsFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        m_sources.clear();
        emit libraryRootChanged();
        emit downloadsRootChanged();
        emit maxConcurrentDownloadsChanged();
        emit sourcesChanged();
        return;
    }

    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    if (obj.contains(QStringLiteral("libraryRoot")))
        m_libraryRoot = obj.value(QStringLiteral("libraryRoot")).toString(m_libraryRoot);
    if (obj.contains(QStringLiteral("downloadsRoot")))
        m_downloadsRoot = obj.value(QStringLiteral("downloadsRoot")).toString(m_downloadsRoot);
    if (obj.contains(QStringLiteral("maxConcurrentDownloads")))
        m_maxConcurrentDownloads = qBound(1, obj.value(QStringLiteral("maxConcurrentDownloads")).toInt(2), 8);

    QVector<SourcePluginInfo> loaded;
    const QJsonArray sources = obj.value(QStringLiteral("sources")).toArray();
    for (const QJsonValue& value : sources) {
        SourcePluginInfo info = sourceFromJson(value.toObject());
        if (!info.id.isEmpty() && !info.name.isEmpty())
            loaded.append(std::move(info));
    }

    // Legacy: only promote old FreeTP URL field if the user actually had one configured.
    bool migrated = false;
    if (loaded.isEmpty()) {
        const QString legacyUrl = obj.value(QStringLiteral("freetpCatalogUrl")).toString().trimmed();
        if (!legacyUrl.isEmpty()) {
            SourcePluginInfo freetp;
            freetp.id = QStringLiteral("freetp");
            freetp.name = QStringLiteral("FreeTP");
            freetp.description =
                QStringLiteral("Торрент-каталог FreeTP — magnet-ссылки и дополнения");
            freetp.catalogUrl = legacyUrl;
            freetp.iconName = QStringLiteral("storefront");
            freetp.enabled = true;
            freetp.capabilities = {QStringLiteral("search"), QStringLiteral("install"),
                                   QStringLiteral("update")};
            loaded.append(std::move(freetp));
            migrated = true;
        }
    }

    m_sources = std::move(loaded);
    if (migrated)
        save();

    emit libraryRootChanged();
    emit downloadsRootChanged();
    emit maxConcurrentDownloadsChanged();
    emit sourcesChanged();
}

void SettingsStore::save()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("libraryRoot"), m_libraryRoot);
    obj.insert(QStringLiteral("downloadsRoot"), m_downloadsRoot);
    obj.insert(QStringLiteral("maxConcurrentDownloads"), m_maxConcurrentDownloads);

    QJsonArray sources;
    for (const auto& source : m_sources)
        sources.append(sourceToJson(source));
    obj.insert(QStringLiteral("sources"), sources);

    QFile file(settingsFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

} // namespace arachnel::core

#include "settings_store.h"

#include "proton_manager.h"
#include "storage_library.h"

#include <QDir>
#include <QFile>
#include <QHash>
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
    , m_libraryRoot(defaultStorageLibraryPath())
    , m_downloadsRoot(defaultStorageLibraryPath() + QStringLiteral("/downloads"))
    , m_maxConcurrentDownloads(2)
    , m_sources(defaultSources())
    , m_storageLibraries(this, this)
{
    ensureDefaultStorageLibraries();
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

void SettingsStore::setPluginEnabledStates(QHash<QString, bool> states)
{
    m_pluginEnabledStates = std::move(states);
    emit pluginStatesChanged();
    save();
}

bool SettingsStore::pluginEnabled(const QString& id, bool defaultValue) const
{
    return m_pluginEnabledStates.value(id, defaultValue);
}

QString defaultLibraryRoot()
{
    return defaultStorageLibraryPath();
}

QString defaultDownloadsRoot()
{
    return defaultStorageLibraryPath() + QStringLiteral("/downloads");
}

void SettingsStore::ensureDefaultStorageLibraries()
{
    if (!m_storageLibraries.libraries().isEmpty())
        return;

    StorageLibrary library;
    library.id = QStringLiteral("default");
    library.path = normalizedStoragePath(m_libraryRoot.isEmpty() ? defaultStorageLibraryPath()
                                                                 : m_libraryRoot);
    library.label = autoStorageLibraryLabel(library.path);
    library.isDefault = true;
    m_storageLibraries.setLibraries({library});
}

QString SettingsStore::resolvedLibraryRoot() const
{
    const QString path = m_storageLibraries.libraryPath(defaultLibraryId());
    if (!path.isEmpty())
        return path;
    return m_libraryRoot.isEmpty() ? defaultStorageLibraryPath() : m_libraryRoot;
}

QString SettingsStore::resolvedDownloadsRoot() const
{
    const QString path = m_storageLibraries.downloadsPath(defaultLibraryId());
    if (!path.isEmpty())
        return path;
    return m_downloadsRoot.isEmpty() ? defaultDownloadsRoot() : m_downloadsRoot;
}

QString SettingsStore::resolvedLibraryRoot(const QString& libraryId) const
{
    const QString id = libraryId.isEmpty() ? defaultLibraryId() : libraryId;
    const QString path = m_storageLibraries.libraryPath(id);
    if (!path.isEmpty())
        return path;
    return resolvedLibraryRoot();
}

QString SettingsStore::resolvedDownloadsRoot(const QString& libraryId) const
{
    const QString id = libraryId.isEmpty() ? defaultLibraryId() : libraryId;
    const QString path = m_storageLibraries.downloadsPath(id);
    if (!path.isEmpty())
        return path;
    return resolvedDownloadsRoot();
}

QString SettingsStore::defaultLibraryId() const
{
    return m_storageLibraries.defaultLibraryId();
}

QString SettingsStore::gameDirFor(const QString& libraryId, const QString& gameId) const
{
    const QString id = libraryId.isEmpty() ? defaultLibraryId() : libraryId;
    return m_storageLibraries.gameDir(id, gameId);
}

void SettingsStore::syncLegacyRootsFromLibrary(const StorageLibrary& library)
{
    const QString gamesRoot = normalizedStoragePath(library.path);
    const QString downloadsRoot = downloadsDirForLibrary(library);
    bool changed = false;
    if (m_libraryRoot != gamesRoot) {
        m_libraryRoot = gamesRoot;
        changed = true;
        emit libraryRootChanged();
    }
    if (m_downloadsRoot != downloadsRoot) {
        m_downloadsRoot = downloadsRoot;
        changed = true;
        emit downloadsRootChanged();
    }
    Q_UNUSED(changed);
}

void SettingsStore::setLibraryRoot(const QString& path)
{
    const QString normalized = normalizedStoragePath(path);
    if (normalized.isEmpty())
        return;

    const QString id = defaultLibraryId();
    if (!id.isEmpty())
        m_storageLibraries.updateLibraryPath(id, normalized);
    else if (m_libraryRoot != normalized) {
        m_libraryRoot = normalized;
        emit libraryRootChanged();
        save();
    }
}

void SettingsStore::setDownloadsRoot(const QString& path)
{
    const QString normalized = normalizedStoragePath(path);
    if (normalized.isEmpty() || m_downloadsRoot == normalized)
        return;
    m_downloadsRoot = normalized;
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

void SettingsStore::setAutoCheckUpdates(bool enabled)
{
    if (m_autoCheckUpdates == enabled)
        return;
    m_autoCheckUpdates = enabled;
    emit autoCheckUpdatesChanged();
    save();
}

void SettingsStore::setAutoInstallUpdates(bool enabled)
{
    if (m_autoInstallUpdates == enabled)
        return;
    m_autoInstallUpdates = enabled;
    emit autoInstallUpdatesChanged();
    save();
}

void SettingsStore::setAutoCheckAppUpdates(bool enabled)
{
    if (m_autoCheckAppUpdates == enabled)
        return;
    m_autoCheckAppUpdates = enabled;
    emit autoCheckAppUpdatesChanged();
    save();
}

void SettingsStore::setUiLanguage(const QString& languageCode)
{
    const QString normalized = languageCode.trimmed().toLower();
    const QString effective = normalized.isEmpty() ? QStringLiteral("en") : normalized;
    if (m_uiLanguage == effective)
        return;
    m_uiLanguage = effective;
    emit uiLanguageChanged();
    save();
}

void SettingsStore::setGlobalLaunchArgs(const QString& args)
{
    if (m_globalLaunchArgs == args)
        return;
    m_globalLaunchArgs = args;
    emit globalLaunchArgsChanged();
    save();
}

void SettingsStore::setDefaultProtonId(const QString& id)
{
    const QString normalized = id.trimmed();
    if (m_defaultProtonId == normalized)
        return;
    m_defaultProtonId = normalized;
    if (!normalized.isEmpty())
        promoteProtonInPriority(normalized);
    emit defaultProtonIdChanged();
    save();
}

void SettingsStore::setProtonPriority(const QStringList& ids)
{
    QStringList normalized;
    normalized.reserve(ids.size());
    for (const QString& id : ids) {
        const QString trimmed = id.trimmed();
        if (!trimmed.isEmpty() && !normalized.contains(trimmed))
            normalized.append(trimmed);
    }
    if (m_protonPriority == normalized)
        return;
    m_protonPriority = normalized;
    emit protonPriorityChanged();
    save();
}

void SettingsStore::promoteProtonInPriority(const QString& id)
{
    const QString normalized = id.trimmed();
    if (normalized.isEmpty())
        return;

    QStringList next = m_protonPriority;
    next.removeAll(normalized);
    next.prepend(normalized);
    setProtonPriority(next);
}

void SettingsStore::clearLegacyProtonPath()
{
    if (m_legacyProtonPath.isEmpty())
        return;
    m_legacyProtonPath.clear();
    save();
}

QString SettingsStore::resolvedProtonId(const QString& gameProtonId,
                                        ProtonManager& manager) const
{
    return manager.resolveProtonId(gameProtonId, m_defaultProtonId, m_protonPriority);
}

void SettingsStore::load()
{
    QFile file(settingsFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        m_sources.clear();
        emit libraryRootChanged();
        emit downloadsRootChanged();
        emit maxConcurrentDownloadsChanged();
        emit autoCheckUpdatesChanged();
        emit autoInstallUpdatesChanged();
        emit autoCheckAppUpdatesChanged();
        emit uiLanguageChanged();
        emit globalLaunchArgsChanged();
        emit defaultProtonIdChanged();
        emit protonPriorityChanged();
        emit sourcesChanged();
        return;
    }

    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    if (obj.contains(QStringLiteral("maxConcurrentDownloads")))
        m_maxConcurrentDownloads = qBound(1, obj.value(QStringLiteral("maxConcurrentDownloads")).toInt(2), 8);
    m_autoCheckUpdates = obj.value(QStringLiteral("autoCheckUpdates")).toBool(true);
    m_autoInstallUpdates = obj.value(QStringLiteral("autoInstallUpdates")).toBool(false);
    m_autoCheckAppUpdates = obj.value(QStringLiteral("autoCheckAppUpdates")).toBool(true);
    m_uiLanguage = obj.value(QStringLiteral("uiLanguage")).toString(QStringLiteral("en")).toLower();
    m_globalLaunchArgs = obj.value(QStringLiteral("globalLaunchArgs")).toString();
    m_defaultProtonId = obj.value(QStringLiteral("defaultProtonId")).toString();
    m_legacyProtonPath = obj.value(QStringLiteral("protonPath")).toString();
    m_protonPriority.clear();
    const QJsonArray priority = obj.value(QStringLiteral("protonPriority")).toArray();
    for (const QJsonValue& value : priority) {
        const QString id = value.toString().trimmed();
        if (!id.isEmpty() && !m_protonPriority.contains(id))
            m_protonPriority.append(id);
    }

    if (obj.contains(QStringLiteral("storageLibraries"))) {
        QVector<StorageLibrary> libraries;
        const QJsonArray storage = obj.value(QStringLiteral("storageLibraries")).toArray();
        libraries.reserve(storage.size());
        for (const QJsonValue& value : storage) {
            const QJsonObject libObj = value.toObject();
            StorageLibrary library;
            library.id = libObj.value(QStringLiteral("id")).toString();
            library.label = libObj.value(QStringLiteral("label")).toString();
            library.path = normalizedStoragePath(libObj.value(QStringLiteral("path")).toString());
            library.isDefault = libObj.value(QStringLiteral("isDefault")).toBool(false);
            if (!library.id.isEmpty() && !library.path.isEmpty())
                libraries.append(std::move(library));
        }
        if (!libraries.isEmpty())
            m_storageLibraries.setLibraries(libraries);
        else
            ensureDefaultStorageLibraries();
    } else {
        if (obj.contains(QStringLiteral("libraryRoot")))
            m_libraryRoot = normalizedStoragePath(
                obj.value(QStringLiteral("libraryRoot")).toString(m_libraryRoot));
        if (obj.contains(QStringLiteral("downloadsRoot")))
            m_downloadsRoot = normalizedStoragePath(
                obj.value(QStringLiteral("downloadsRoot")).toString(m_downloadsRoot));
        ensureDefaultStorageLibraries();
    }

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

    m_pluginEnabledStates.clear();
    const QJsonObject pluginStates = obj.value(QStringLiteral("pluginStates")).toObject();
    for (auto it = pluginStates.constBegin(); it != pluginStates.constEnd(); ++it) {
        if (it.value().isObject())
            m_pluginEnabledStates.insert(it.key(), it.value().toObject().value(QStringLiteral("enabled")).toBool(true));
        else
            m_pluginEnabledStates.insert(it.key(), it.value().toBool(true));
    }

    if (migrated)
        save();

    emit libraryRootChanged();
    emit downloadsRootChanged();
    emit maxConcurrentDownloadsChanged();
    emit autoCheckUpdatesChanged();
    emit autoInstallUpdatesChanged();
    emit autoCheckAppUpdatesChanged();
    emit uiLanguageChanged();
    emit globalLaunchArgsChanged();
    emit defaultProtonIdChanged();
    emit protonPriorityChanged();
    emit sourcesChanged();
}

void SettingsStore::save()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("libraryRoot"), m_libraryRoot);
    obj.insert(QStringLiteral("downloadsRoot"), m_downloadsRoot);
    obj.insert(QStringLiteral("maxConcurrentDownloads"), m_maxConcurrentDownloads);
    obj.insert(QStringLiteral("autoCheckUpdates"), m_autoCheckUpdates);
    obj.insert(QStringLiteral("autoInstallUpdates"), m_autoInstallUpdates);
    obj.insert(QStringLiteral("autoCheckAppUpdates"), m_autoCheckAppUpdates);
    obj.insert(QStringLiteral("uiLanguage"), m_uiLanguage);
    obj.insert(QStringLiteral("globalLaunchArgs"), m_globalLaunchArgs);
    obj.insert(QStringLiteral("defaultProtonId"), m_defaultProtonId);
    QJsonArray priority;
    for (const QString& id : m_protonPriority)
        priority.append(id);
    obj.insert(QStringLiteral("protonPriority"), priority);

    QJsonArray storageLibraries;
    for (const auto& library : m_storageLibraries.libraries()) {
        QJsonObject libObj;
        libObj.insert(QStringLiteral("id"), library.id);
        libObj.insert(QStringLiteral("label"), library.label);
        libObj.insert(QStringLiteral("path"), library.path);
        libObj.insert(QStringLiteral("isDefault"), library.isDefault);
        storageLibraries.append(libObj);
    }
    obj.insert(QStringLiteral("storageLibraries"), storageLibraries);

    QJsonArray sources;
    for (const auto& source : m_sources)
        sources.append(sourceToJson(source));
    obj.insert(QStringLiteral("sources"), sources);

    QJsonObject pluginStates;
    for (auto it = m_pluginEnabledStates.constBegin(); it != m_pluginEnabledStates.constEnd(); ++it) {
        QJsonObject state;
        state.insert(QStringLiteral("enabled"), it.value());
        pluginStates.insert(it.key(), state);
    }
    obj.insert(QStringLiteral("pluginStates"), pluginStates);

    QFile file(settingsFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

} // namespace arachnel::core

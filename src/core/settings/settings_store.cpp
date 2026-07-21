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


#include "settings_store_helpers.h"

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

void SettingsStore::setOnboardingCompleted(bool completed)
{
    if (m_onboardingCompleted == completed)
        return;
    m_onboardingCompleted = completed;
    emit onboardingCompletedChanged();
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

void SettingsStore::setSteamInstallMode(const QString& mode)
{
    const QString normalized = mode.trimmed().toLower();
    QString next;
    if (normalized == QStringLiteral("ddmod") || normalized == QStringLiteral("native"))
        next = normalized;
    if (m_steamInstallMode == next)
        return;
    m_steamInstallMode = next;
    emit steamInstallModeChanged();
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

} // namespace arachnel::core

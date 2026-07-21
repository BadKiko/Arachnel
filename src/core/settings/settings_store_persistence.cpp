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

void SettingsStore::load()
{
    QFile file(settingsFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        m_sources.clear();
        m_onboardingCompleted = false;
        emit libraryRootChanged();
        emit downloadsRootChanged();
        emit maxConcurrentDownloadsChanged();
        emit autoCheckUpdatesChanged();
        emit autoInstallUpdatesChanged();
        emit autoCheckAppUpdatesChanged();
        emit uiLanguageChanged();
        emit onboardingCompletedChanged();
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
    // Existing installs without the key skip the wizard; only true first launch shows it.
    m_onboardingCompleted = obj.contains(QStringLiteral("onboardingCompleted"))
                                ? obj.value(QStringLiteral("onboardingCompleted")).toBool(false)
                                : true;
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
    emit onboardingCompletedChanged();
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
    obj.insert(QStringLiteral("onboardingCompleted"), m_onboardingCompleted);
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

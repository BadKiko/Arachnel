#include "core_controller_impl.h"

namespace arachnel::core {

bool CoreController::ensureRuntimeDependenciesForGame(const LibraryGame& game)
{
#if !defined(Q_OS_LINUX)
    // Proton prefix / Wine redistributables are a Linux concern. On Windows the OS
    // already hosts native .exe games — do not block Play on runtime container setup.
    Q_UNUSED(game);
    return true;
#else
    if (!m_runtimeDependencyService)
        return true;

    QString steamAppId = game.steamAppId;
    if (steamAppId.isEmpty()) {
        if (const CatalogEntry* entry = findCatalogEntry(game.id))
            steamAppId = entry->steamAppId;
    }
    if (steamAppId.isEmpty() && m_metadataService) {
        const GameMetadata metadata = m_metadataService->metadataForTitle(game.title);
        steamAppId = metadata.steamAppId;
    }

    RuntimeEnsureRequest request;
    request.gameId = game.id;
    request.steamAppId = steamAppId;
    request.title = game.title;
    request.installPath = game.installPath;

    setRuntimeSetupActive(game,
                          QCoreApplication::translate("Core", "Preparing runtime environment…"));

    const RuntimeEnsureResult result = m_runtimeDependencyService->ensureInstalled(
        request, m_protonManager, &m_settings, [this](const QString& status) {
            if (status.isEmpty())
                return;
            // Queue UI updates — never processEvents() from inside launch/setup.
            QMetaObject::invokeMethod(
                this,
                [this, status]() {
                    m_runtimeSetupStatus = status;
                    emit runtimeSetupChanged();
                },
                Qt::QueuedConnection);
        });

    clearRuntimeSetup();

    if (!result.success && !result.error.isEmpty()) {
        showNotice(result.error);
        return false;
    }
    return true;
#endif
}

void CoreController::setRuntimeSetupActive(const LibraryGame& game, const QString& status)
{
    m_runtimeSetupInProgress = true;
    m_runtimeSetupGameId = game.id;
    m_runtimeSetupTitle = game.title;
    m_runtimeSetupCoverUrl = game.coverUrl;
    m_runtimeSetupStatus = status;
    emit runtimeSetupChanged();
}

void CoreController::clearRuntimeSetup()
{
    if (!m_runtimeSetupInProgress && m_runtimeSetupGameId.isEmpty())
        return;
    m_runtimeSetupInProgress = false;
    m_runtimeSetupGameId.clear();
    m_runtimeSetupTitle.clear();
    m_runtimeSetupCoverUrl.clear();
    m_runtimeSetupStatus.clear();
    emit runtimeSetupChanged();
}

void CoreController::refreshAvailableProtons()
{
    if (!m_protonManager)
        return;
    m_protonManager->availableEntries(true);
    syncProtonCatalog();
    emit availableProtonsChanged();
}

void CoreController::moveProtonInPriority(const QString& protonId, int delta)
{
    const QString id = protonId.trimmed();
    if (id.isEmpty() || delta == 0)
        return;

    QStringList priority = m_settings.protonPriority();
    const int index = priority.indexOf(id);
    if (index < 0)
        return;

    const int target = index + delta;
    if (target < 0 || target >= priority.size())
        return;

    priority.move(index, target);
    m_settings.setProtonPriority(priority);
}

QString CoreController::protonNameForId(const QString& protonId) const
{
    return m_protonManager ? m_protonManager->nameForId(protonId) : QString();
}

QVariantList CoreController::availableProtons() const
{
    QVariantList result;
    if (!m_protonManager)
        return result;

    const QString defaultId = m_settings.defaultProtonId();
    const QStringList priority = m_settings.protonPriority();
    QSet<QString> emitted;

    const auto appendEntry = [&](const ProtonEntry& entry) {
        if (emitted.contains(entry.id))
            return;
        emitted.insert(entry.id);
        result.append(QVariantMap{
            {QStringLiteral("id"), entry.id},
            {QStringLiteral("name"), entry.name},
            {QStringLiteral("source"), entry.source},
            {QStringLiteral("sourceLabel"), entry.sourceLabel},
            {QStringLiteral("installDir"), entry.installDir},
            {QStringLiteral("isDefault"), entry.id == defaultId},
            {QStringLiteral("priorityIndex"), priority.indexOf(entry.id)},
        });
    };

    for (const QString& id : priority) {
        for (const ProtonEntry& entry : m_protonManager->availableEntries()) {
            if (entry.id == id)
                appendEntry(entry);
        }
    }

    for (const ProtonEntry& entry : m_protonManager->availableEntries())
        appendEntry(entry);

    return result;
}

void CoreController::syncProtonCatalog()
{
    if (!m_protonManager)
        return;

    const QVector<ProtonEntry> entries = m_protonManager->availableEntries(true);
    QStringList priority = m_settings.protonPriority();
    bool priorityChanged = false;

    for (const ProtonEntry& entry : entries) {
        if (!priority.contains(entry.id)) {
            priority.append(entry.id);
            priorityChanged = true;
        }
    }

    if (priorityChanged)
        m_settings.setProtonPriority(priority);

    if (!m_settings.legacyProtonPath().isEmpty()) {
        const QString legacyId = m_protonManager->idForInstallDir(m_settings.legacyProtonPath());
        if (!legacyId.isEmpty() && m_settings.defaultProtonId().isEmpty())
            m_settings.setDefaultProtonId(legacyId);
        m_settings.clearLegacyProtonPath();
    }

    if (m_settings.defaultProtonId().isEmpty() && !entries.isEmpty())
        m_settings.setDefaultProtonId(entries.first().id);
}

void CoreController::downloadProtonGe()
{
    if (!m_protonManager)
        return;
    m_protonManager->downloadLatestGe();
}

void CoreController::refreshProtonLatestRelease()
{
    if (m_protonManager)
        m_protonManager->refreshLatestGeRelease();
}

bool CoreController::needsProtonOnPlatform() const
{
#if defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

bool CoreController::protonReady() const
{
    if (!needsProtonOnPlatform())
        return true;
    if (!m_protonManager)
        return false;
    const QString protonId = m_settings.resolvedProtonId(QString(), *m_protonManager);
    return !m_protonManager->executableForId(protonId).isEmpty();
}

QString CoreController::protonVersion() const
{
    if (!m_protonManager)
        return {};
    const QString protonId = m_settings.resolvedProtonId(QString(), *m_protonManager);
    return m_protonManager->nameForId(protonId);
}

QString CoreController::protonLatestRelease() const
{
    return m_protonManager ? m_protonManager->latestGeReleaseName() : QString();
}

bool CoreController::ensureProtonReady()
{
    if (protonReady())
        return true;

    refreshProtonLatestRelease();

    const QString latest = protonLatestRelease();
    if (latest.isEmpty()) {
        showNotice(QCoreApplication::translate(
            "Core", "Install Proton-GE in Settings → Launch before downloading games"));
    } else {
        showNotice(QCoreApplication::translate(
            "Core", "Install %1 (Proton-GE) in Settings → Launch before downloading games")
                       .arg(latest));
    }
    return false;
}

bool CoreController::protonDownloadInProgress() const
{
    return m_protonManager && m_protonManager->isDownloading();
}

int CoreController::protonDownloadProgress() const
{
    return m_protonManager ? m_protonManager->downloadProgress() : 0;
}

QString CoreController::protonDownloadStatus() const
{
    return m_protonManager ? m_protonManager->downloadStatus() : QString();
}

} // namespace arachnel::core

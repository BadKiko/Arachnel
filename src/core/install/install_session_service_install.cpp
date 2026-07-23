#include "install_session_service.h"

#include "file_utils.h"
#include "install_marker.h"
#include "job_model.h"
#include "job_orchestrator.h"
#include "plugin_host.h"
#include "plugin_interface.h"
#include "proton_manager.h"
#include "settings_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QtConcurrent>

namespace arachnel::core {

namespace {

bool pathIsUnderRoot(const QString& path, const QString& root)
{
    if (path.isEmpty() || root.isEmpty())
        return false;
    const QString cleanPath = QDir::cleanPath(path);
    const QString cleanRoot = QDir::cleanPath(root);
    if (cleanPath.compare(cleanRoot, Qt::CaseInsensitive) == 0)
        return true;
    const QString prefix = cleanRoot.endsWith(QLatin1Char('/')) ? cleanRoot
                                                                : cleanRoot + QLatin1Char('/');
    return cleanPath.startsWith(prefix, Qt::CaseInsensitive);
}

} // namespace

void InstallSessionService::cleanupDownloadArtifact(const QString& artifactPath,
                                                    const QString& installPath,
                                                    const QString& libraryId) const
{
    if (!m_settings || artifactPath.isEmpty())
        return;

    const QString artifact = QDir::cleanPath(artifactPath);
    const QString install = QDir::cleanPath(installPath);
    if (artifact.isEmpty() || !QFileInfo::exists(artifact))
        return;
    // Never wipe the live install tree (steamidra writes straight into target).
    if (!install.isEmpty()
        && (artifact.compare(install, Qt::CaseInsensitive) == 0
            || pathIsUnderRoot(install, artifact) || pathIsUnderRoot(artifact, install)))
        return;

    const QString downloadsRoot =
        QDir::cleanPath(m_settings->resolvedDownloadsRoot(libraryId));
    if (!pathIsUnderRoot(artifact, downloadsRoot))
        return;

    // Large FreeTP payloads — don't block the UI thread.
    (void)QtConcurrent::run([artifact]() { removePathRecursive(artifact); });
}

void InstallSessionService::startPluginAddonInstall(const CatalogEntry& parent,
                                                    const CatalogComponent& addon,
                                                    const QString& sourceId,
                                                    const QString& artifactPath,
                                                    const QString& progressJobId,
                                                    std::function<void(bool)> done)
{
    const QString installKey = parent.id + QLatin1Char(':') + addon.id;
    if (m_installingAddons.contains(installKey)) {
        m_hooks.showNotice(QCoreApplication::translate(
                               "Core", "Add-on installation is already in progress"),
                           true);
        return;
    }
    const LibraryGame* game = m_libraryStore->gameById(parent.id);
    if (!game || game->installPath.isEmpty()) {
        m_hooks.showNotice(QCoreApplication::translate("Core", "Install the game first"), true);
        return;
    }
    ISourcePlugin* plugin = m_pluginHost ? m_pluginHost->plugin(sourceId) : nullptr;
    if (!plugin) {
        m_hooks.showNotice(
            QCoreApplication::translate(
                "Core", "Plugin not found for %1 — install it in Settings → Plugins")
                .arg(sourceId),
            true);
        return;
    }

    m_installingAddons.insert(installKey);
    const QVariantMap addonJobMap = m_jobs->jobForAddon(parent.id, addon.id);
    const QString addonJobId = addonJobMap.value(QStringLiteral("jobId")).toString();
    if (!addonJobId.isEmpty()) {
        m_jobOrchestrator->setJobPhase(addonJobId, QStringLiteral("installing"),
                                       QStringLiteral("Installing add-on…"));
    } else if (!progressJobId.isEmpty() && !m_installSessions.contains(parent.id)) {
        m_jobOrchestrator->setJobPhase(progressJobId, QStringLiteral("installing"),
                                       QStringLiteral("Installing add-on…"));
    }
    if (m_installSessions.contains(parent.id))
        syncInstallSessionPhase(parent.id);

    AddonInstallContext ctx;
    ctx.parentEntryId = parent.id;
    ctx.addonId = addon.id;
    ctx.addonTitle = addon.title;
    ctx.gameInstallPath = game->installPath;
    ctx.downloadPath = artifactPath;
    ctx.addonKind = addon.kind;
    m_hooks.fillProtonInstallFields(
        parent.id, m_settings->resolvedProtonId(QString(), *m_protonManager),
        &ctx.protonExecutable, &ctx.compatDataPath, &ctx.steamCompatClientPath);

    const QString gameInstallPath = game->installPath;
    const QString gameLibraryId = game->libraryId;

    m_pluginHost->runAddonInstallAsync(
        plugin, ctx,
        [this, parent, addon, progressJobId, installKey, done, artifactPath, gameInstallPath,
         gameLibraryId](const InstallResult& result) {
            m_installingAddons.remove(installKey);
            if (!result.success) {
                const QString detail = result.error.isEmpty()
                                           ? QStringLiteral("Install failed")
                                           : QStringLiteral("Install failed: %1").arg(result.error);
                const QVariantMap addonJobMap = m_jobs->jobForAddon(parent.id, addon.id);
                const QString addonJobId = addonJobMap.value(QStringLiteral("jobId")).toString();
                if (!addonJobId.isEmpty())
                    m_jobOrchestrator->setJobPhase(addonJobId, QStringLiteral("completed"), detail);
                if (!progressJobId.isEmpty() && progressJobId != addonJobId)
                    m_jobOrchestrator->setJobPhase(progressJobId, QStringLiteral("completed"), detail);
                clearSession(parent.id);
                m_hooks.showNotice(
                    QCoreApplication::translate("Core", "Add-on install failed for %1: %2")
                        .arg(addon.title, result.error),
                    true);
                if (done)
                    done(false);
                return;
            }

            m_hooks.markAddonInstalled(parent.id, addon.id, addon.uploadDate);
            cleanupDownloadArtifact(artifactPath, gameInstallPath, gameLibraryId);
            const QVariantMap successJobMap = m_jobs->jobForAddon(parent.id, addon.id);
            const QString successJobId = successJobMap.value(QStringLiteral("jobId")).toString();
            if (!successJobId.isEmpty())
                m_jobOrchestrator->setJobPhase(successJobId, QStringLiteral("completed"),
                                               QStringLiteral("Installed"));
            m_hooks.showNotice(QCoreApplication::translate("Core", "Add-on installed: %1")
                                   .arg(addon.title),
                               true);
            if (done)
                done(true);
        });
}

void InstallSessionService::commitInstalledCatalogGame(const CatalogEntry& entryHint,
                                                       const QString& sourceId,
                                                       const QString& savePath,
                                                       const QString& libraryId,
                                                       const QString& installPath,
                                                       InstallKind installKind)
{
    const CatalogEntry* fresh = m_hooks.findCatalogEntry(entryHint.id);
    const CatalogEntry& catalog = fresh ? *fresh : entryHint;
    const QString libId = libraryId.isEmpty() ? m_settings->defaultLibraryId() : libraryId;
    LibraryGame game;
    if (const LibraryGame* existing = m_libraryStore->gameById(catalog.id))
        game = *existing;

    game.id = catalog.id;
    game.title = catalog.title;
    game.coverUrl = catalog.coverUrl;
    game.sourceId = sourceId;
    game.sourceName = m_hooks.sourceNameForId(sourceId);
    game.version = catalog.version;
    game.description = catalog.description;
    game.genres = catalog.genres;
    game.sizeLabel = catalog.sizeLabel;
    game.installKind = installKind;
    game.uploadDate = catalog.uploadDate;
    game.magnetUri = catalog.magnetUris.value(0);
    game.downloadPath = savePath;
    game.libraryId = libId;
    game.hasUpdate = false;
    if (!installPath.isEmpty()) {
        const QString previousInstall = game.installPath;
        game.installPath = installPath;
        const QString override = game.executableOverride.trimmed();
        const QString cleanInstall = QDir::cleanPath(installPath);
        const QString cleanOverride = QDir::cleanPath(override);
        const bool overrideInsideInstall =
            !override.isEmpty() && cleanOverride.startsWith(cleanInstall, Qt::CaseInsensitive)
            && (cleanOverride.size() == cleanInstall.size()
                || cleanOverride.at(cleanInstall.size()) == QLatin1Char('/')
                || cleanOverride.at(cleanInstall.size()) == QLatin1Char('\\'));
        const bool installChanged =
            previousInstall.isEmpty()
            || QDir::cleanPath(previousInstall).compare(cleanInstall, Qt::CaseInsensitive) != 0;
        if (override.isEmpty() || !overrideInsideInstall || installChanged) {
            const QString executable = m_hooks.findGameExecutable(installPath);
            if (!executable.isEmpty())
                game.executableOverride = executable;
            else if (!overrideInsideInstall)
                game.executableOverride.clear();
        }
    }

    QHash<QString, InstalledComponent> previousComponents;
    for (const auto& component : game.components)
        previousComponents.insert(component.id, component);
    QVector<InstalledComponent> components;
    components.reserve(catalog.addons.size());
    for (const auto& addon : catalog.addons) {
        InstalledComponent component{addon.id, addon.title, addon.uploadDate};
        if (const auto it = previousComponents.constFind(addon.id); it != previousComponents.cend())
            component.installed = it->installed;
        components.append(component);
    }
    game.components = components;
    if (game.steamAppId.isEmpty() && !catalog.steamAppId.isEmpty())
        game.steamAppId = catalog.steamAppId;
    if (game.steamAppId.isEmpty())
        game.steamAppId = m_hooks.metadataSteamAppIdForTitle(catalog.title);

    if (!game.installPath.isEmpty() && QFileInfo::exists(game.installPath))
        writeInstallMarker(game.installPath, game.id, game.sourceId);

    // Drop installer/torrent payload once the game is in the library.
    cleanupDownloadArtifact(savePath, game.installPath, libId);
    if (!savePath.isEmpty()
        && QDir::cleanPath(savePath).compare(QDir::cleanPath(game.installPath),
                                             Qt::CaseInsensitive)
            != 0)
        game.downloadPath.clear();

    m_libraryStore->upsertGame(game);
    m_hooks.syncLibrary();
    m_hooks.recalculateLibraryUpdates();
    m_hooks.gameCommitted(game);
}

} // namespace arachnel::core

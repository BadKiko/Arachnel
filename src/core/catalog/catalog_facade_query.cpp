#include "core_controller_impl.h"
namespace arachnel::core {

void CoreController::searchCatalog(const QString& sourceId, const QString& query)
{
    const SourcePluginInfo* source = m_sources.pluginById(sourceId);
    if (!source) {
        showNotice(QCoreApplication::translate("Core", "Unknown source: %1").arg(sourceId));
        return;
    }
    if (!source->enabled) {
        showNotice(QCoreApplication::translate("Core", "Source \"%1\" is disabled in settings").arg(source->name));
        return;
    }

    if (m_catalogController)
        m_catalogController->selectCatalogSource(sourceId, query);
}

void CoreController::setActiveCatalogSource(const QString& sourceId)
{
    if (m_catalogController)
        m_catalogController->setActiveCatalogSource(sourceId);
}

bool CoreController::isCatalogSourceSelected(const QString& sourceId) const
{
    return m_catalogController && m_catalogController->isCatalogSourceSelected(sourceId);
}

void CoreController::toggleCatalogSource(const QString& sourceId)
{
    if (m_catalogController)
        m_catalogController->toggleCatalogSource(sourceId);
}

void CoreController::applyCatalogSearch(const QString& query)
{
    if (m_catalogController)
        m_catalogController->applyCatalogSearch(query);
}

void CoreController::pruneDisabledCatalogSources()
{
    if (m_catalogController)
        m_catalogController->pruneDisabledCatalogSources();
}

void CoreController::selectCatalogSource(const QString& sourceId, const QString& query)
{
    if (m_catalogController)
        m_catalogController->selectCatalogSource(sourceId, query);
}

void CoreController::clearCatalogView()
{
    if (m_catalogController)
        m_catalogController->clearCatalogView();
}

int CoreController::catalogEntryCount(const QString& sourceId) const
{
    return m_catalogController ? m_catalogController->catalogEntryCount(sourceId) : -1;
}

void CoreController::invalidateSourceCatalog(const QString& sourceId)
{
    if (m_catalogController)
        m_catalogController->invalidateSourceCatalog(sourceId);
}

void CoreController::prefetchCatalogCounts()
{
    if (m_catalogController)
        m_catalogController->prefetchCatalogCounts();
}

void CoreController::validateHydraCatalogUrl(const QString& requestId, const QString& url)
{
    const QString trimmed = url.trimmed();
    if (requestId.isEmpty() || trimmed.isEmpty()) {
        emit hydraCatalogUrlValidated(requestId, false, 0,
                                      QCoreApplication::translate("Core", "Enter a catalog URL"));
        return;
    }

    const QUrl parsed(trimmed);
    if (!parsed.isValid() || !parsed.scheme().startsWith(QStringLiteral("http"),
                                                         Qt::CaseInsensitive)) {
        emit hydraCatalogUrlValidated(requestId, false, 0,
                                      QCoreApplication::translate("Core", "Invalid URL - http or https required"));
        return;
    }

    m_catalogValidateLoader->loadFeed(parsed, QStringLiteral("validate:%1").arg(requestId));
}

void CoreController::refreshCatalog(const QString& sourceId)
{
    if (m_catalogController)
        m_catalogController->refreshCatalog(sourceId);
}

void CoreController::refreshSelectedCatalogs()
{
    if (m_catalogController)
        m_catalogController->refreshSelectedCatalogs();
}

void registerCoreTypes()
{
    qmlRegisterSingletonType<CoreController>("Arachnel.Core", 1, 0, "Core", &CoreController::create);
    qmlRegisterUncreatableType<LibraryModel>("Arachnel.Core", 1, 0, "LibraryModel",
                                             QStringLiteral("Use Core.library"));
    qmlRegisterUncreatableType<SourcePluginModel>("Arachnel.Core", 1, 0, "SourcePluginModel",
                                                  QStringLiteral("Use Core.sources"));
    qmlRegisterUncreatableType<CatalogModel>("Arachnel.Core", 1, 0, "CatalogModel",
                                             QStringLiteral("Use Core.catalog"));
    qmlRegisterUncreatableType<JobModel>("Arachnel.Core", 1, 0, "JobModel",
                                         QStringLiteral("Use Core.jobs"));
    qmlRegisterUncreatableType<NotificationModel>("Arachnel.Core", 1, 0, "NotificationModel",
                                                  QStringLiteral("Use Core.notifications"));
    qmlRegisterUncreatableType<SettingsStore>("Arachnel.Core", 1, 0, "SettingsStore",
                                              QStringLiteral("Use Core.settings"));
    qmlRegisterUncreatableType<AppUpdater>("Arachnel.Core", 1, 0, "AppUpdater",
                                           QStringLiteral("Use Core.appUpdater"));
    qmlRegisterUncreatableType<PluginCatalogService>("Arachnel.Core", 1, 0, "PluginCatalogService",
                                                     QStringLiteral("Use Core.pluginCatalog"));
    qmlRegisterUncreatableType<StorageLibraryModel>("Arachnel.Core", 1, 0, "StorageLibraryModel",
                                                    QStringLiteral("Use Core.settings.storageLibraries"));
}

} // namespace arachnel::core

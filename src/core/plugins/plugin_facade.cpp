#include "core_controller_impl.h"

namespace arachnel::core {

void CoreController::syncSourcesFromPlugins()
{
    QVector<SourcePluginInfo> merged = m_pluginHost->pluginInfos();
    for (auto& info : merged)
        info.enabled = m_settings.pluginEnabled(info.id, true);

    for (const auto& manual : m_settings.sources()) {
        if (m_pluginHost->hasPlugin(manual.id))
            continue;
        merged.append(manual);
    }

    m_sources.setPlugins(merged);
}

void CoreController::persistSourcesToSettings()
{
    QHash<QString, bool> pluginStates;
    QVector<SourcePluginInfo> manualSources;
    for (const auto& source : m_sources.plugins()) {
        if (source.isPlugin)
            pluginStates.insert(source.id, source.enabled);
        else
            manualSources.append(source);
    }
    m_settings.setPluginEnabledStates(pluginStates);
    m_settings.persistSources(manualSources);
}

void CoreController::applyPluginCatalog(const QString& sourceId,
                                        QVector<CatalogEntry> entries)
{
    if (m_catalogController)
        m_catalogController->commitCatalogLoad(sourceId, std::move(entries));
}

void CoreController::startPluginInstall(const CatalogEntry& entry, const QString& sourceId,
                                        const QString& savePath, JobKind kind,
                                        const QString& libraryId, const QString& jobId)
{
    m_installSessionService->startPluginInstall(entry, sourceId, savePath, kind, libraryId, jobId);
}

void CoreController::startPluginAddonInstall(const CatalogEntry& parent, const CatalogComponent& addon,
                                             const QString& sourceId, const QString& artifactPath,
                                             const QString& progressJobId,
                                             std::function<void(bool)> done)
{
    m_installSessionService->startPluginAddonInstall(parent, addon, sourceId, artifactPath,
                                                     progressJobId, std::move(done));
}

void CoreController::beginInstallSession(const QString& entryId, const QString& gameJobId,
                                         const QString& sourceId, const QStringList& addonIds)
{
    m_installSessionService->beginInstallSession(entryId, gameJobId, sourceId, addonIds);
}

int CoreController::pluginCount() const
{
    return m_pluginHost ? m_pluginHost->count() : 0;
}

QString CoreController::pluginsUserDir() const
{
    return PluginHost::writablePluginsDir();
}

QString CoreController::pluginsBundleDir() const
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/plugins");
}

QVariantList CoreController::pluginEntries() const
{
    QVariantList entries;
    for (const auto& source : m_sources.plugins()) {
        if (!source.isPlugin)
            continue;
        QVariantMap row;
        row.insert(QStringLiteral("pluginId"), source.id);
        row.insert(QStringLiteral("name"), source.name);
        row.insert(QStringLiteral("description"), source.description);
        row.insert(QStringLiteral("pluginVersion"), source.pluginVersion);
        row.insert(QStringLiteral("pluginRootPath"), source.pluginRootPath);
        row.insert(QStringLiteral("sourceEnabled"), source.enabled);
        entries.append(row);
    }
    return entries;
}

bool CoreController::isPluginInstalledOnDisk(const QString& pluginId) const
{
    if (!m_pluginHost)
        return false;
    if (m_pluginHost->hasPlugin(pluginId))
        return true;
    return m_pluginHost->hasPluginFilesOnDisk(pluginId);
}

bool CoreController::installPluginArach(const QUrl& fileUrl)
{
    if (!m_pluginHost)
        return false;

    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    const bool ok = m_pluginHost->installFromArach(path);
    m_lastPluginError = ok ? QString() : m_pluginHost->lastError();
    emit lastPluginErrorChanged();
    if (ok) {
        showNotice(QCoreApplication::translate("Core", "Plugin installed"));
        emit pluginsChanged();
    } else {
        showNotice(QCoreApplication::translate("Core", "Plugin install failed: %1").arg(m_lastPluginError));
    }
    return ok;
}

bool CoreController::uninstallPlugin(const QString& pluginId)
{
    if (!m_pluginHost)
        return false;

    const bool ok = m_pluginHost->uninstallPlugin(pluginId);
    m_lastPluginError = ok ? QString() : m_pluginHost->lastError();
    emit lastPluginErrorChanged();
    if (ok) {
        showNotice(QCoreApplication::translate("Core", "Plugin removed"));
        // PluginHost::scan() already emits pluginsChanged → syncSourcesFromPlugins.
    } else {
        showNotice(QCoreApplication::translate("Core", "Could not remove plugin: %1")
                       .arg(m_lastPluginError));
    }
    return ok;
}

void CoreController::refreshOfficialPlugins()
{
    if (m_pluginCatalog)
        m_pluginCatalog->refresh();
}

void CoreController::installOfficialPlugin(const QString& pluginId)
{
    if (m_pluginCatalog)
        m_pluginCatalog->installPlugin(pluginId);
}

void CoreController::browsePluginArach()
{
#if defined(Q_OS_WIN)
    QString path;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool comOwned = SUCCEEDED(hr);

    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&dialog)))) {
        const COMDLG_FILTERSPEC filters[] = {
            {L"Пакет плагина (*.arach)", L"*.arach"},
        };
        dialog->SetFileTypes(1, filters);
        dialog->SetTitle(L"Установить плагин");
        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR widePath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &widePath))) {
                    path = QString::fromWCharArray(widePath);
                    CoTaskMemFree(widePath);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (comOwned)
        CoUninitialize();

    if (!path.isEmpty())
        installPluginArach(QUrl::fromLocalFile(path));
#else
    const QString path = QFileDialog::getOpenFileName(
        nullptr,
        QCoreApplication::translate("Core", "Install plugin"),
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
        QCoreApplication::translate("Core", "Plugin files (*.arach)"));
    if (!path.isEmpty())
        installPluginArach(QUrl::fromLocalFile(path));
#endif
}

void CoreController::openPluginsFolder()
{
    if (!m_pluginHost)
        return;
    if (!PluginHost::openWritablePluginsDir())
        showNotice(QCoreApplication::translate("Core", "Could not open plugins folder"));
}

void CoreController::rescanPlugins()
{
    if (!m_pluginHost)
        return;
    m_pluginHost->scan();
}

} // namespace arachnel::core

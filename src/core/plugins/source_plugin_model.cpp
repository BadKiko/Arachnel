#include "source_plugin_model.h"

#include <QRegularExpression>

namespace arachnel::core {

namespace {

QStringList defaultCapabilities()
{
    return {QStringLiteral("search"), QStringLiteral("install"), QStringLiteral("update")};
}

} // namespace

QString slugifySourceId(const QString& name)
{
    QString slug = name.trimmed().toLower();
    slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    slug.replace(QRegularExpression(QStringLiteral("^-+|-+$")), QString());
    if (slug.isEmpty())
        slug = QStringLiteral("source");
    return slug;
}

QVector<SourcePluginInfo> defaultSources()
{
    return {};
}

SourcePluginModel::SourcePluginModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int SourcePluginModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_plugins.size();
}

QVariant SourcePluginModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_plugins.size())
        return {};

    const auto& plugin = m_plugins.at(index.row());
    switch (role) {
    case PluginIdRole:
        return plugin.id;
    case NameRole:
        return plugin.name;
    case DescriptionRole:
        return plugin.description;
    case CatalogUrlRole:
        return plugin.catalogUrl;
    case IconNameRole:
        return plugin.iconName.isEmpty() ? QStringLiteral("storefront") : plugin.iconName;
    case SourceEnabledRole:
        return plugin.enabled;
    case CapabilitiesRole:
        return plugin.capabilities;
    case HasCatalogUrlRole:
        return plugin.isPlugin || !plugin.catalogUrl.trimmed().isEmpty();
    case IsPluginRole:
        return plugin.isPlugin;
    case PluginVersionRole:
        return plugin.pluginVersion;
    case PluginRootPathRole:
        return plugin.pluginRootPath;
    default:
        return {};
    }
}

QHash<int, QByteArray> SourcePluginModel::roleNames() const
{
    return {
        {PluginIdRole, "pluginId"},
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {CatalogUrlRole, "catalogUrl"},
        {IconNameRole, "iconName"},
        {SourceEnabledRole, "sourceEnabled"},
        {CapabilitiesRole, "capabilities"},
        {HasCatalogUrlRole, "hasCatalogUrl"},
        {IsPluginRole, "isPlugin"},
        {PluginVersionRole, "pluginVersion"},
        {PluginRootPathRole, "pluginRootPath"},
    };
}

void SourcePluginModel::setPlugins(QVector<SourcePluginInfo> plugins)
{
    beginResetModel();
    m_plugins = std::move(plugins);
    endResetModel();
    emit sourcesChanged();
}

const SourcePluginInfo* SourcePluginModel::pluginById(const QString& id) const
{
    for (const auto& plugin : m_plugins) {
        if (plugin.id == id)
            return &plugin;
    }
    return nullptr;
}

SourcePluginInfo* SourcePluginModel::pluginByIdMutable(const QString& id)
{
    for (auto& plugin : m_plugins) {
        if (plugin.id == id)
            return &plugin;
    }
    return nullptr;
}

int SourcePluginModel::enabledCount() const
{
    int count = 0;
    for (const auto& plugin : m_plugins) {
        if (plugin.enabled)
            ++count;
    }
    return count;
}

QString SourcePluginModel::firstEnabledId() const
{
    for (const auto& plugin : m_plugins) {
        if (plugin.enabled)
            return plugin.id;
    }
    return {};
}

QString SourcePluginModel::catalogUrlFor(const QString& id) const
{
    const SourcePluginInfo* plugin = pluginById(id);
    return plugin ? plugin->catalogUrl : QString();
}

QString SourcePluginModel::nameForId(const QString& id) const
{
    const SourcePluginInfo* plugin = pluginById(id);
    return plugin ? plugin->name : id;
}

bool SourcePluginModel::isSourceEnabled(const QString& id) const
{
    const SourcePluginInfo* plugin = pluginById(id);
    return plugin && plugin->enabled;
}

QString SourcePluginModel::uniqueIdFromName(const QString& name) const
{
    const QString base = slugifySourceId(name);
    QString candidate = base;
    int suffix = 2;
    while (pluginById(candidate)) {
        candidate = QStringLiteral("%1-%2").arg(base).arg(suffix++);
    }
    return candidate;
}

QString SourcePluginModel::addSource(const QString& name, const QString& catalogUrl,
                                     const QString& description, const QString& iconName)
{
    const QString trimmedName = name.trimmed();
    const QString trimmedUrl = catalogUrl.trimmed();
    if (trimmedName.isEmpty() || trimmedUrl.isEmpty())
        return {};

    SourcePluginInfo info;
    info.id = uniqueIdFromName(trimmedName);
    info.name = trimmedName;
    info.description = description.trimmed();
    info.catalogUrl = trimmedUrl;
    info.iconName = iconName.trimmed().isEmpty() ? QStringLiteral("storefront") : iconName.trimmed();
    info.enabled = true;
    info.isPlugin = false;
    info.capabilities = defaultCapabilities();

    const int row = m_plugins.size();
    beginInsertRows({}, row, row);
    m_plugins.append(std::move(info));
    endInsertRows();
    emit sourcesChanged();
    return m_plugins.last().id;
}

bool SourcePluginModel::updateSource(const QString& id, const QString& name,
                                     const QString& catalogUrl, const QString& description,
                                     const QString& iconName, bool enabled)
{
    SourcePluginInfo* plugin = pluginByIdMutable(id);
    if (!plugin || plugin->isPlugin)
        return false;

    const QString trimmedName = name.trimmed();
    const QString trimmedUrl = catalogUrl.trimmed();
    if (trimmedName.isEmpty() || trimmedUrl.isEmpty())
        return false;

    plugin->name = trimmedName;
    plugin->catalogUrl = trimmedUrl;
    plugin->description = description.trimmed();
    plugin->iconName =
        iconName.trimmed().isEmpty() ? QStringLiteral("storefront") : iconName.trimmed();
    plugin->enabled = enabled;

    int row = -1;
    for (int i = 0; i < m_plugins.size(); ++i) {
        if (m_plugins.at(i).id == id) {
            row = i;
            break;
        }
    }
    if (row < 0)
        return false;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
    emit sourcesChanged();
    return true;
}

bool SourcePluginModel::removeSource(const QString& id)
{
    for (int i = 0; i < m_plugins.size(); ++i) {
        if (m_plugins.at(i).id != id)
            continue;
        if (m_plugins.at(i).isPlugin)
            return false;
        beginRemoveRows({}, i, i);
        m_plugins.removeAt(i);
        endRemoveRows();
        emit sourcesChanged();
        return true;
    }
    return false;
}

bool SourcePluginModel::setSourceEnabled(const QString& id, bool enabled)
{
    SourcePluginInfo* plugin = pluginByIdMutable(id);
    if (!plugin || plugin->enabled == enabled)
        return false;

    plugin->enabled = enabled;
    int row = -1;
    for (int i = 0; i < m_plugins.size(); ++i) {
        if (m_plugins.at(i).id == id) {
            row = i;
            break;
        }
    }
    if (row < 0)
        return false;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {SourceEnabledRole});
    emit sourcesChanged();
    return true;
}

} // namespace arachnel::core

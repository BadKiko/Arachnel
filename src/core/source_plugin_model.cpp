#include "source_plugin_model.h"

namespace arachnel::core {

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
    case CapabilitiesRole:
        return plugin.capabilities;
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
        {CapabilitiesRole, "capabilities"},
    };
}

void SourcePluginModel::setPlugins(QVector<SourcePluginInfo> plugins)
{
    beginResetModel();
    m_plugins = std::move(plugins);
    endResetModel();
}

const SourcePluginInfo* SourcePluginModel::pluginById(const QString& id) const
{
    for (const auto& plugin : m_plugins) {
        if (plugin.id == id)
            return &plugin;
    }
    return nullptr;
}

QString SourcePluginModel::nameForId(const QString& id) const
{
    const SourcePluginInfo* plugin = pluginById(id);
    return plugin ? plugin->name : id;
}

} // namespace arachnel::core

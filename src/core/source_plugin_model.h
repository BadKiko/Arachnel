#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QStringList>

namespace arachnel::core {

struct SourcePluginInfo {
    QString id;
    QString name;
    QString description;
    QStringList capabilities;
};

class SourcePluginModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        PluginIdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        CapabilitiesRole,
    };
    Q_ENUM(Role)

    explicit SourcePluginModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setPlugins(QVector<SourcePluginInfo> plugins);
    const SourcePluginInfo* pluginById(const QString& id) const;
    Q_INVOKABLE QString nameForId(const QString& id) const;

private:
    QVector<SourcePluginInfo> m_plugins;
};

} // namespace arachnel::core

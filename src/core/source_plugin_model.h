#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QVector>

namespace arachnel::core {

struct SourcePluginInfo {
    QString id;
    QString name;
    QString description;
    QString catalogUrl;
    QString iconName;
    bool enabled = true;
    bool isPlugin = false;
    QString pluginVersion;
    QString pluginRootPath;
    QStringList capabilities;
};

QString slugifySourceId(const QString& name);
QVector<SourcePluginInfo> defaultSources();

class SourcePluginModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY sourcesChanged)
    Q_PROPERTY(int enabledCount READ enabledCount NOTIFY sourcesChanged)
    Q_PROPERTY(QString firstEnabledId READ firstEnabledId NOTIFY sourcesChanged)

public:
    enum Role {
        PluginIdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        CatalogUrlRole,
        IconNameRole,
        SourceEnabledRole,
        CapabilitiesRole,
        HasCatalogUrlRole,
        IsPluginRole,
        PluginVersionRole,
        PluginRootPathRole,
    };
    Q_ENUM(Role)

    explicit SourcePluginModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setPlugins(QVector<SourcePluginInfo> plugins);
    QVector<SourcePluginInfo> plugins() const { return m_plugins; }
    const SourcePluginInfo* pluginById(const QString& id) const;
    SourcePluginInfo* pluginByIdMutable(const QString& id);

    int count() const { return m_plugins.size(); }
    int enabledCount() const;
    QString firstEnabledId() const;
    QString catalogUrlFor(const QString& id) const;

    Q_INVOKABLE QString nameForId(const QString& id) const;
    Q_INVOKABLE bool isSourceEnabled(const QString& id) const;
    Q_INVOKABLE QString addSource(const QString& name, const QString& catalogUrl,
                                  const QString& description = {},
                                  const QString& iconName = {});
    Q_INVOKABLE bool updateSource(const QString& id, const QString& name,
                                  const QString& catalogUrl, const QString& description,
                                  const QString& iconName, bool enabled);
    Q_INVOKABLE bool removeSource(const QString& id);
    Q_INVOKABLE bool setSourceEnabled(const QString& id, bool enabled);

signals:
    void sourcesChanged();

private:
    QString uniqueIdFromName(const QString& name) const;

    QVector<SourcePluginInfo> m_plugins;
};

} // namespace arachnel::core

#pragma once

#include <QObject>
#include <QVariantList>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace arachnel::core {

class PluginCatalogService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool installing READ installing NOTIFY installingChanged)
    Q_PROPERTY(QString installingPluginId READ installingPluginId NOTIFY installingChanged)
    Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString catalogUrl READ catalogUrl CONSTANT)

public:
    explicit PluginCatalogService(QObject* parent = nullptr);

    QVariantList plugins() const { return m_plugins; }
    bool loading() const { return m_loading; }
    bool installing() const { return m_installing; }
    QString installingPluginId() const { return m_installingPluginId; }
    int downloadProgress() const { return m_downloadProgress; }
    QString error() const { return m_error; }
    QString catalogUrl() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void installPlugin(const QString& pluginId);

signals:
    void pluginsChanged();
    void loadingChanged();
    void installingChanged();
    void downloadProgressChanged();
    void errorChanged();
    /// On success, pathOrError is the downloaded .arach path; on failure it is an error message.
    void installFinished(const QString& pluginId, bool ok, const QString& pathOrError);
    /// Fired when the install queue becomes idle (no active download and no pending ids).
    void installQueueDrained();

private:
    void setLoading(bool value);
    void setInstalling(const QString& pluginId);
    void setDownloadProgress(int percent);
    void setError(const QString& message);
    void downloadAndInstall(const QVariantMap& entry);
    void finishInstallAttempt(const QString& pluginId, bool ok, const QString& pathOrError);
    void processInstallQueue();
    bool beginInstall(const QString& pluginId);
    QString downloadUrlForEntry(const QVariantMap& entry) const;
    QString currentPlatformId() const;

    QNetworkAccessManager* m_network = nullptr;
    QNetworkReply* m_catalogReply = nullptr;
    QNetworkReply* m_downloadReply = nullptr;
    QVariantList m_plugins;
    QStringList m_installQueue;
    bool m_loading = false;
    bool m_installing = false;
    int m_downloadProgress = 0;
    QString m_installingPluginId;
    QString m_error;
    QString m_pendingInstallId;
};

} // namespace arachnel::core

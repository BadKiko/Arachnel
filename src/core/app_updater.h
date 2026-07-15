#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace arachnel::core {

class AppUpdater : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool checking READ checking NOTIFY stateChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY stateChanged)
    Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
    Q_PROPERTY(QString releasePageUrl READ releasePageUrl NOTIFY stateChanged)

public:
    explicit AppUpdater(QObject* parent = nullptr);

    bool checking() const { return m_checking; }
    bool updateAvailable() const { return m_updateAvailable; }
    bool downloading() const { return m_downloading; }
    int downloadProgress() const { return m_downloadProgress; }
    QString currentVersion() const;
    QString latestVersion() const { return m_latestVersion; }
    QString statusText() const { return m_statusText; }
    QString lastError() const { return m_lastError; }
    QString releasePageUrl() const { return m_releasePageUrl; }

    Q_INVOKABLE void checkForUpdates(bool notifyIfUpToDate = true);
    Q_INVOKABLE void downloadAndInstall();
    Q_INVOKABLE void openReleasePage();

signals:
    void stateChanged();
    void downloadProgressChanged();
    void updateCheckFinished(bool available, const QString& latestVersion);
    void updateFailed(const QString& error);
    void installerLaunchRequested();

private:
    void setChecking(bool value);
    void setDownloading(bool value);
    void setStatusText(const QString& text);
    void setLastError(const QString& error);
    void handleReleasePayload(const QByteArray& payload, bool notifyIfUpToDate);
    void startDownload(const QUrl& url);
    bool launchInstaller(const QString& installerPath, QString* errorOut);
    static int compareVersions(const QString& left, const QString& right);
    static QString normalizeVersion(const QString& version);

    QNetworkAccessManager* m_network = nullptr;
    QNetworkReply* m_activeReply = nullptr;
    bool m_checking = false;
    bool m_updateAvailable = false;
    bool m_downloading = false;
    int m_downloadProgress = 0;
    QString m_latestVersion;
    QString m_downloadUrl;
    QString m_releasePageUrl;
    QString m_statusText;
    QString m_lastError;
    qint64 m_downloadBytesTotal = 0;
};

} // namespace arachnel::core

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace arachnel::core {

struct ProtonEntry {
    QString id;
    QString name;
    QString installDir;
    QString source;      // arachnel | steam | steam-tool
    QString sourceLabel; // display hint for UI
};

class ProtonManager : public QObject
{
    Q_OBJECT

public:
    explicit ProtonManager(QObject* parent = nullptr);

    QString protonInstallRoot() const;
    QString compatDataRoot() const;

    QVector<ProtonEntry> availableEntries(bool forceRescan = false) const;
    QStringList installedVersions() const;
    QString resolveProtonExecutable(const QString& preferredIdOrLegacyPath = {}) const;
    QString resolveProtonId(const QString& gameProtonId, const QString& defaultProtonId,
                            const QStringList& priorityIds) const;
    QString executableForId(const QString& id) const;
    QString installDirForId(const QString& id) const;
    QString nameForId(const QString& id) const;
    QString idForInstallDir(const QString& installDir) const;
    QString activeVersionName(const QString& preferredIdOrLegacyPath = {}) const;
    bool isAvailable(const QString& preferredIdOrLegacyPath = {}) const;
    QString latestGeReleaseName() const { return m_latestGeReleaseName; }
    QString steamCompatClientPath() const;
    QString compatDataPathForGame(const QString& gameId) const;
    /** Path to SteamLinuxRuntime_sniper/run (or soldier) when installed; empty otherwise. */
    QString findSteamLinuxRuntime() const;

    bool isDownloading() const { return m_downloading; }
    int downloadProgress() const { return m_downloadProgress; }
    QString downloadStatus() const { return m_downloadStatus; }

    void refreshLatestGeRelease();
    void downloadLatestGe();
    void invalidateScanCache();

signals:
    void downloadStateChanged();
    void downloadFinished(bool success, const QString& error);
    void versionsChanged();
    void latestGeReleaseChanged();
    void availableEntriesChanged();

private:
    void scanEntries(QVector<ProtonEntry>* out) const;
    void setDownloadProgress(int percent, const QString& status);
    void finishDownload(bool success, const QString& error = {});
    bool extractTarGz(const QString& archivePath, const QString& destDir, QString* errorOut);
    QString findProtonScriptInDir(const QString& dir) const;
    bool fetchLatestGeReleaseInfo(QString* versionNameOut, QString* downloadUrlOut);
    QString makeEntryId(const QString& source, const QString& installDir) const;
    void appendEntry(QVector<ProtonEntry>* out, const QString& source,
                     const QString& sourceLabel, const QString& installDir,
                     const QString& displayName) const;
    QStringList steamRoots() const;
    QStringList steamLibraryRoots(const QString& steamRoot) const;

    mutable QVector<ProtonEntry> m_cachedEntries;
    mutable bool m_cacheValid = false;

    bool m_downloading = false;
    int m_downloadProgress = 0;
    QString m_downloadStatus;
    QString m_latestGeReleaseName;
};

} // namespace arachnel::core

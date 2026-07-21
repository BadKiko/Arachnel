#pragma once

#include "catalog_types.h"
#include "install_analysis.h"
#include "install_kind.h"
#include "library_model.h"

#include <QString>
#include <QStringList>
#include <QHash>
#include <QVector>
#include <functional>
#include <optional>

namespace arachnel::core {

struct LaunchInfo {
    QString executable;
    QString workingDirectory;
    QStringList arguments;
    QStringList argumentsPrefix;
    QString wineDllOverrides;
    QHash<QString, QString> environmentExtras;
};

struct InstallContext {
    QString jobId;
    QString entryId;
    QString sourceId;
    QString title;
    QString targetPath;
    QString downloadsPath;
    QString downloadPath;
    QString magnetUri;
    QString uploadDate;
    InstallKind installKind = InstallKind::PortableArchive;
    QString protonExecutable;
    QString compatDataPath;
    QString steamCompatClientPath;
    /** Optional plugin hint (e.g. steamidra: "ddmod" | "native" | "auto"). */
    QString installMode;
};

struct InstallResult {
    bool success = false;
    QString installPath;
    QString error;
};

/** Progress for plugins with capability `owns_download` (API v3). */
struct OwnedDownloadProgress {
    int percent = 0;
    qint64 bytesDownloaded = 0;
    qint64 totalBytes = 0;
    int downloadRateBps = 0; // bytes/sec; 0 = unknown
    QString detail;
    QString status; // downloading | installing | paused | …
};

struct AddonInstallContext {
    QString parentEntryId;
    QString addonId;
    QString addonTitle;
    QString gameInstallPath;
    QString downloadPath;
    CatalogItemKind addonKind = CatalogItemKind::Addon;
    QString protonExecutable;
    QString compatDataPath;
    QString steamCompatClientPath;
};

class ISourcePlugin
{
public:
    virtual ~ISourcePlugin() = default;

    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QString version() const { return QStringLiteral("1.0.0"); }
    virtual QStringList capabilities() const = 0;

    virtual QVector<CatalogEntry> catalog() const = 0;
    virtual QVector<CatalogEntry> search(const QString& query) const = 0;
    virtual std::optional<CatalogEntry> entryById(const QString& entryId) const = 0;

    virtual InstallResult installFromDownload(const InstallContext& ctx) const = 0;

    virtual InstallResult installAddonFromDownload(const AddonInstallContext& ctx) const
    {
        (void)ctx;
        InstallResult result;
        result.success = false;
        result.error = QStringLiteral("Add-ons are not supported by this source");
        return result;
    }

    virtual InstallAnalysis analyzeDownload(const InstallContext& ctx) const = 0;

    virtual InstallAnalysis analyzeFileNames(const QStringList& fileNames) const = 0;

    virtual std::optional<QString> detectUpdate(const LibraryGame& local,
                                                const CatalogEntry& remote) const = 0;

    virtual LaunchInfo launchInfo(const LibraryGame& local) const = 0;

    virtual void resetCatalogCache() {}

    /**
     * Plugin-owned download+install (API v3). Default: not supported.
     * Only called when capabilities() contains "owns_download" and apiVersion >= 3.
     * Report progress via onProgress; return final InstallResult when finished.
     */
    virtual InstallResult startOwnedDownload(
        const InstallContext& ctx,
        const std::function<void(const OwnedDownloadProgress&)>& onProgress) const
    {
        (void)ctx;
        (void)onProgress;
        InstallResult result;
        result.success = false;
        result.error = QStringLiteral("Plugin-owned download is not supported");
        return result;
    }

    virtual void cancelOwnedDownload(const QString& jobId) const
    {
        (void)jobId;
    }
};

} // namespace arachnel::core

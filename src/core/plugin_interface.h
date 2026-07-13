#pragma once

#include "catalog_types.h"
#include "install_kind.h"
#include "library_model.h"

#include <QString>
#include <QStringList>
#include <QHash>
#include <QVector>
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
};

struct InstallResult {
    bool success = false;
    QString installPath;
    QString error;
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
        result.error = QStringLiteral("Дополнения не поддерживаются этим источником");
        return result;
    }

    virtual InstallKind detectInstallKind(const QString& downloadPath) const
    {
        (void)downloadPath;
        return InstallKind::PortableArchive;
    }

    virtual InstallKind detectInstallKindFromFileNames(const QStringList& fileNames) const
    {
        (void)fileNames;
        return InstallKind::PortableArchive;
    }

    virtual std::optional<QString> detectUpdate(const LibraryGame& local,
                                                const CatalogEntry& remote) const = 0;

    virtual LaunchInfo launchInfo(const LibraryGame& local) const = 0;

    virtual void resetCatalogCache() {}
};

} // namespace arachnel::core

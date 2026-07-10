#pragma once

#include "catalog_types.h"
#include "install_kind.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

namespace arachnel::core {

struct LaunchInfo {
    QString executable;
    QString workingDirectory;
    QStringList arguments;
};

struct InstallContext {
    QString jobId;
    QString entryId;
    QString sourceId;
    QString targetPath;
    QString downloadsPath;
    QString magnetUri;
    QString uploadDate;
    InstallKind installKind = InstallKind::PortableArchive;
};

struct LibraryGame;

class ISourcePlugin
{
public:
    virtual ~ISourcePlugin() = default;

    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QStringList capabilities() const = 0;
    virtual QString catalogFeedUrl() const { return {}; }

    virtual QVector<CatalogEntry> search(const QVector<CatalogEntry>& catalog,
                                         const QString& query) const = 0;
    virtual std::optional<CatalogEntry> metadata(const QVector<CatalogEntry>& catalog,
                                                 const QString& entryId) const = 0;

    virtual void install(const InstallContext& ctx) = 0;
    virtual void cancelInstall(const QString& jobId) = 0;

    virtual std::optional<QString> detectUpdate(const LibraryGame& local,
                                                const CatalogEntry& remote) const = 0;
    virtual void update(const InstallContext& ctx, const LibraryGame& local) = 0;

    virtual LaunchInfo launchInfo(const LibraryGame& local) const = 0;
};

} // namespace arachnel::core

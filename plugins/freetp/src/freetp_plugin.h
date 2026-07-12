#pragma once

#include "plugin_interface.h"

#include <QString>
#include <QVector>

namespace freetp {

class FreetpPlugin final : public arachnel::core::ISourcePlugin
{
public:
    explicit FreetpPlugin(QString rootPath);

    QString id() const override;
    QString name() const override;
    QString description() const override;
    QString version() const override;
    QStringList capabilities() const override;

    QVector<arachnel::core::CatalogEntry> catalog() const override;
    QVector<arachnel::core::CatalogEntry> search(const QString& query) const override;
    std::optional<arachnel::core::CatalogEntry> entryById(const QString& entryId) const override;

    arachnel::core::InstallResult installFromDownload(
        const arachnel::core::InstallContext& ctx) const override;

    arachnel::core::InstallResult installAddonFromDownload(
        const arachnel::core::AddonInstallContext& ctx) const override;

    arachnel::core::InstallKind detectInstallKind(const QString& downloadPath) const override;

    arachnel::core::InstallKind detectInstallKindFromFileNames(
        const QStringList& fileNames) const override;

    std::optional<QString> detectUpdate(const arachnel::core::LibraryGame& local,
                                        const arachnel::core::CatalogEntry& remote) const override;

    arachnel::core::LaunchInfo launchInfo(const arachnel::core::LibraryGame& local) const override;

    void resetCatalogCache() override;

private:
    void ensureCatalogLoaded() const;

    QString m_rootPath;
    mutable QVector<arachnel::core::CatalogEntry> m_catalog;
    mutable bool m_catalogLoaded = false;
    mutable bool m_forceRemoteCatalog = false;
};

} // namespace freetp

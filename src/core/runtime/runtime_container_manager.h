#pragma once

#include <QString>
#include <QStringList>

namespace arachnel::core {

class RuntimeContainerManager
{
public:
    QString containerRootForGame(const QString& gameId) const;
    QString cacheDirForGame(const QString& gameId) const;
    QString stateFileForGame(const QString& gameId) const;
    QString prefixDirForGame(const QString& gameId) const;

    QStringList installedDepotIds(const QString& gameId) const;
    void markDepotInstalled(const QString& gameId, const QString& steamAppId,
                            const QString& depotId);
    void unmarkDepotInstalled(const QString& gameId, const QString& depotId);
    bool isDepotInstalledForGame(const QString& gameId, const QString& depotId) const;
};

} // namespace arachnel::core

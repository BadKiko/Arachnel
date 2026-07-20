#pragma once

#include "runtime_dependency_types.h"

#include <QObject>
#include <QVariantMap>
#include <functional>

class QNetworkAccessManager;

namespace arachnel::core {

class ProtonManager;
class SettingsStore;

class RuntimeDependencyService : public QObject
{
    Q_OBJECT

public:
    explicit RuntimeDependencyService(QObject* parent = nullptr);

    QVector<RuntimeDepotRef> resolveDependencies(const QString& steamAppId,
                                                 QString* errorOut = nullptr) const;

    RuntimeEnsureResult ensureInstalled(const RuntimeEnsureRequest& request,
                                        ProtonManager* protonManager,
                                        SettingsStore* settings,
                                        const std::function<void(const QString&)>& onStatus =
                                            {}) const;

    /** Linux: container paths + runtime dependency status for game settings. */
    QVariantMap containerInfoForGame(const RuntimeEnsureRequest& request) const;

private:
    mutable QNetworkAccessManager* m_network = nullptr;

    QNetworkAccessManager* network() const;

    QVector<RuntimeDepotRef> resolveFromSteamCmd(const QString& steamAppId,
                                                 QString* errorOut) const;
    QVector<RuntimeDepotRef> mergeDependencies(const QVector<RuntimeDepotRef>& primary,
                                                 const QVector<RuntimeDepotRef>& extra) const;

    bool isDepotSatisfied(const RuntimeDepotRef& depot, const QString& gameId) const;
    bool installDepotIntoContainer(const RuntimeDepotRef& depot, const RuntimeEnsureRequest& request,
                                   ProtonManager* protonManager, SettingsStore* settings,
                                   const std::function<void(const QString&)>& onStatus,
                                   QString* errorOut) const;
};

} // namespace arachnel::core

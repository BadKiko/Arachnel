#include "runtime_dependency_service.h"

#include "proton_manager.h"
#include "runtime_container_manager.h"
#include "install_heuristics.h"
#include "runtime_depot_catalog.h"
#include "runtime_manifest_probe.h"
#include "settings_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QWaitCondition>

#include <memory>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

namespace arachnel::core {


#include "runtime_dependency_service_helpers.h"

RuntimeDependencyService::RuntimeDependencyService(QObject* parent)
    : QObject(parent)
{
}

QNetworkAccessManager* RuntimeDependencyService::network() const
{
    if (!m_network)
        m_network = new QNetworkAccessManager(const_cast<RuntimeDependencyService*>(this));
    return m_network;
}

QVector<RuntimeDepotRef> RuntimeDependencyService::resolveFromSteamCmd(const QString& steamAppId,
                                                                       QString* errorOut) const
{
    QVector<RuntimeDepotRef> result;
    const QUrl url(QString::fromUtf8(kSteamCmdInfo) + steamAppId);
    const QByteArray body = httpGetBlocking(network(), url, 20000, errorOut);
    if (body.isEmpty())
        return result;

    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const QJsonObject app = data.value(steamAppId).toObject();
    const QJsonObject depots = app.value(QStringLiteral("depots")).toObject();

    for (auto it = depots.begin(); it != depots.end(); ++it) {
        const QString depotId = it.key();
        if (!depotId.toInt())
            continue;
        const QJsonObject block = it.value().toObject();
        const bool shared = block.value(QStringLiteral("sharedinstall")).toString()
                                == QStringLiteral("1")
                            || block.value(QStringLiteral("sharedinstall")).toBool();
        const QString fromApp = block.value(QStringLiteral("depotfromapp")).toString();
        if (!shared && !RuntimeDepotCatalog::isSteamworksSharedDepot(depotId))
            continue;
        if (!fromApp.isEmpty() && fromApp != QStringLiteral("228980")
            && !RuntimeDepotCatalog::isSteamworksSharedDepot(depotId))
            continue;

        RuntimeDepotRef ref;
        ref.depotId = depotId;
        ref.label = RuntimeDepotCatalog::labelForDepotId(depotId);
        const QJsonObject config = block.value(QStringLiteral("config")).toObject();
        ref.osList = config.value(QStringLiteral("oslist")).toString();
        result.append(ref);
    }
    return result;
}

QVector<RuntimeDepotRef> RuntimeDependencyService::mergeDependencies(
    const QVector<RuntimeDepotRef>& primary, const QVector<RuntimeDepotRef>& extra) const
{
    QVector<RuntimeDepotRef> merged = primary;
    for (const RuntimeDepotRef& ref : extra) {
        bool found = false;
        for (const RuntimeDepotRef& existing : merged) {
            if (existing.depotId == ref.depotId) {
                found = true;
                break;
            }
        }
        if (!found)
            merged.append(ref);
    }
    return merged;
}

QVector<RuntimeDepotRef> RuntimeDependencyService::resolveDependencies(const QString& steamAppId,
                                                                       QString* errorOut) const
{
    if (steamAppId.trimmed().isEmpty()) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Steam App ID is missing");
        return {};
    }

    QString steamCmdError;
    QVector<RuntimeDepotRef> deps = resolveFromSteamCmd(steamAppId, &steamCmdError);

    if (deps.isEmpty() && !steamCmdError.isEmpty() && errorOut)
        *errorOut = steamCmdError;

    return deps;
}

bool RuntimeDependencyService::isDepotSatisfied(const RuntimeDepotRef& depot,
                                                const QString& gameId) const
{
    RuntimeContainerManager containers;
    const QString prefixDir = containers.prefixDirForGame(gameId);

#if defined(Q_OS_WIN)
    if (RuntimeDepotCatalog::isVcDepotId(depot.depotId)) {
        if (isVcRedist2015Installed(RuntimeDepotCatalog::isX64VcDepotId(depot.depotId)))
            return true;
    } else if (depot.depotId == QStringLiteral("228990")) {
        if (isDepotInstalledInRegistry(depot.depotId))
            return true;
    } else if (containers.isDepotInstalledForGame(gameId, depot.depotId)) {
        return true;
    }
#else
    if (isDepotInstalledInPrefix(depot, prefixDir))
        return true;
#endif

    if (containers.isDepotInstalledForGame(gameId, depot.depotId))
        containers.unmarkDepotInstalled(gameId, depot.depotId);

    return false;
}


} // namespace arachnel::core

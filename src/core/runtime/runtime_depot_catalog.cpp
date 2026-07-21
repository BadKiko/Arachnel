#include "runtime_depot_catalog.h"

namespace arachnel::core {

QString RuntimeDepotCatalog::labelForDepotId(const QString& depotId)
{
    if (isSteamworksSharedDepot(depotId))
        return QStringLiteral("Steamworks runtime (%1)").arg(depotId);
    return QStringLiteral("Runtime (%1)").arg(depotId);
}

bool RuntimeDepotCatalog::isSteamworksSharedDepot(const QString& depotId)
{
    bool ok = false;
    const int n = depotId.toInt(&ok);
    if (!ok)
        return false;
    return n >= 228980 && n <= 229099;
}

bool RuntimeDepotCatalog::isVcDepotId(const QString& depotId)
{
    bool ok = false;
    const int id = depotId.toInt(&ok);
    return ok && id >= 228981 && id <= 228989;
}

bool RuntimeDepotCatalog::isX64VcDepotId(const QString& depotId)
{
    return depotId == QStringLiteral("228982") || depotId == QStringLiteral("228984")
           || depotId == QStringLiteral("228986") || depotId == QStringLiteral("228988")
           || depotId == QStringLiteral("228989");
}

} // namespace arachnel::core

#pragma once

#include <QString>

namespace arachnel::core {

class RuntimeDepotCatalog
{
public:
    static QString labelForDepotId(const QString& depotId);
    static bool isSteamworksSharedDepot(const QString& depotId);
    static bool isVcDepotId(const QString& depotId);
    static bool isX64VcDepotId(const QString& depotId);
};

} // namespace arachnel::core

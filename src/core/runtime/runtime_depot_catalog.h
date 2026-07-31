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
    /** .NET Framework redists (from Steam InstallScripts path under DotNet/). */
    static bool isDotNetDepotId(const QString& depotId);
    /** OpenAL (from Steam InstallScripts path under OpenAL/). */
    static bool isOpenALDepotId(const QString& depotId);
};

} // namespace arachnel::core

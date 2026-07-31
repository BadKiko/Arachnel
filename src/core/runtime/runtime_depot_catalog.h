#pragma once

#include <QString>

namespace arachnel::core {

class RuntimeDepotCatalog
{
public:
    static QString labelForDepotId(const QString& depotId);
    static bool isSteamworksSharedDepot(const QString& depotId);
    static bool isVcDepotId(const QString& depotId);
    /** Steamworks VC 2015+ depots that need unified 2015-2022 aka.ms packages. */
    static bool isModernVcDepotId(const QString& depotId);
    static bool isX64VcDepotId(const QString& depotId);
    /** .NET Framework redists (from Steam InstallScripts path under DotNet/). */
    static bool isDotNetDepotId(const QString& depotId);
    /** OpenAL (from Steam InstallScripts path under OpenAL/). */
    static bool isOpenALDepotId(const QString& depotId);
};

} // namespace arachnel::core

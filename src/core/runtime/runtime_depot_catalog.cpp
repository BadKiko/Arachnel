#include "runtime_depot_catalog.h"

#include "steamworks_install_scripts.h"

namespace arachnel::core {
namespace {

QString installScriptPathLower(const QString& depotId)
{
    return installScriptRelativePathForDepot(depotId).replace(QLatin1Char('\\'), QLatin1Char('/'))
        .toLower();
}

} // namespace

QString RuntimeDepotCatalog::labelForDepotId(const QString& depotId)
{
    // Prefer Steam's own InstallScripts path (live ACF, else bundled map from Valve).
    const QString fromSteam =
        labelFromInstallScriptRelativePath(installScriptRelativePathForDepot(depotId));
    if (!fromSteam.isEmpty())
        return fromSteam;

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

bool RuntimeDepotCatalog::isModernVcDepotId(const QString& depotId)
{
    // Steam 2015/2017/2019/2022 shared depots - use unified aka.ms 2015-2022 redist.
    return depotId == QStringLiteral("228986") || depotId == QStringLiteral("228987")
           || depotId == QStringLiteral("228988") || depotId == QStringLiteral("228989");
}

bool RuntimeDepotCatalog::isVcDepotId(const QString& depotId)
{
    const QString path = installScriptPathLower(depotId);
    if (path.contains(QStringLiteral("/vcredist/")))
        return true;
    bool ok = false;
    const int id = depotId.toInt(&ok);
    return ok && id >= 228981 && id <= 228989;
}

bool RuntimeDepotCatalog::isX64VcDepotId(const QString& depotId)
{
    // Modern unified depots install both arches in Steam; CRT probe/CDN prefer x64.
    if (isModernVcDepotId(depotId))
        return true;
    // Legacy Steam pair-ish IDs we still treat as x64 when CDN is used.
    return depotId == QStringLiteral("228982") || depotId == QStringLiteral("228984");
}

bool RuntimeDepotCatalog::isDotNetDepotId(const QString& depotId)
{
    return installScriptPathLower(depotId).contains(QStringLiteral("/dotnet/"));
}

bool RuntimeDepotCatalog::isOpenALDepotId(const QString& depotId)
{
    return installScriptPathLower(depotId).contains(QStringLiteral("/openal/"));
}

} // namespace arachnel::core

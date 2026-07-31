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
    // Steam InstallScripts don't split arch per depot; these IDs need the x64 CRT / CDN package.
    return depotId == QStringLiteral("228982") || depotId == QStringLiteral("228984")
           || depotId == QStringLiteral("228986") || depotId == QStringLiteral("228988")
           || depotId == QStringLiteral("228989");
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

#pragma once

#include "runtime_dependency_types.h"

#include <QString>

namespace arachnel::core {

/** Absolute path to .../steamapps/common/Steamworks Shared when present. */
QString findSteamworksSharedInstallDir();

/** Absolute path to .../Steamworks Shared/_CommonRedist when present. */
QString findSteamworksCommonRedistRoot();

/**
 * Resolve InstallScripts path for a depot (live appmanifest_228980.acf, else bundled map).
 * Relative paths use Steam's backslash style under Steamworks Shared.
 */
QString installScriptRelativePathForDepot(const QString& depotId);

/**
 * Human label from Steam InstallScripts path, e.g.
 * `_CommonRedist/DotNet/4.8/installscript.vdf` → `.NET Framework 4.8`.
 */
QString labelFromInstallScriptRelativePath(const QString& relativePath);

/**
 * Build a runnable plan: script path + Run Process steps with %INSTALLDIR% resolved.
 * Returns empty steps if script or installers are missing.
 */
RedistInstallPlan buildRedistInstallPlan(const QString& depotId);

} // namespace arachnel::core

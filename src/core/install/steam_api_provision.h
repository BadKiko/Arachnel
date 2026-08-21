#pragma once

#include <QString>

namespace arachnel::core {

// Add-only: copy matching-arch steam_api(.dll)/steam_api64.dll next to the exe when missing.
// Fixes Mono/Steamworks.NET DllNotFoundException. Never overwrites existing files.
QString ensureSteamApiDllForExecutable(const QString& installPath, const QString& executablePath);

} // namespace arachnel::core

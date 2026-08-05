#pragma once

#include "plugin_interface.h"

#include <QString>
#include <QVariantMap>

namespace arachnel::core {

/** On-disk Online Fix (Goldberg steam_api or legacy SteamFix/winmm). */
struct OnlineFixOverlayState {
    bool present = false;  // active or disabled overlay / Goldberg files found
    bool enabled = false;  // emu live or legacy DLLs active (not renamed off)
    QString overlayDir;
};

OnlineFixOverlayState detectOnlineFixOverlay(const QString& installPath);
/** Enable/disable Online Fix (Goldberg marker/Valve restore, or legacy *.arachnel-off). */
bool setOnlineFixOverlayEnabled(const QString& installPath, bool enabled, QString* error = nullptr);
/** Labels + flags for Game Settings / entryDetails. */
QVariantMap onlineFixOverlayInfo(const QString& installPath);

/**
 * Proton / Wine launch extras for SteamFix / Online-Fix overlays (SOFL-compatible).
 * Sets WINEDLLOVERRIDES, optional legacy steam-runtime/run.sh, and LD_PRELOAD
 * gameoverlayrenderer (+ SteamAppId/SteamGameId). Safe no-op when overlay is missing/disabled.
 */
void applyOnlineFixLaunchInfo(const QString& installPath, LaunchInfo* info);

#if defined(Q_OS_LINUX)
bool isSteamClientRunning();
/** Best-effort: spawn `steam` detached. Returns true if the process was started. */
bool tryStartSteamClient();
#endif

} // namespace arachnel::core

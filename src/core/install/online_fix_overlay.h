#pragma once

#include "plugin_interface.h"

#include <QString>
#include <QVariantMap>

namespace arachnel::core {

/** On-disk Online Fix (SteamFix / winmm, or older valve steam_api backups). */
struct OnlineFixOverlayState {
    bool present = false;  // active or disabled overlay / marker found
    bool enabled = false;  // overlay DLLs active (not renamed *.arachnel-off)
    QString overlayDir;
};

OnlineFixOverlayState detectOnlineFixOverlay(const QString& installPath);
/** Enable/disable Online Fix (SteamFix rename, or marker + Valve restore for old embeds). */
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

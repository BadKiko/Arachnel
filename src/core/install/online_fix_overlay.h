#pragma once

#include "plugin_interface.h"

#include <QString>
#include <QVariantMap>

namespace arachnel::core {

/** On-disk SteamFix / winmm overlay next to a game install. */
struct OnlineFixOverlayState {
    bool present = false;  // active or disabled overlay files found
    bool enabled = false;  // winmm/SteamFix DLLs are active (not renamed off)
    QString overlayDir;
};

OnlineFixOverlayState detectOnlineFixOverlay(const QString& installPath);
/** Enable/disable by renaming overlay DLLs (*.arachnel-off). Returns false on I/O error. */
bool setOnlineFixOverlayEnabled(const QString& installPath, bool enabled, QString* error = nullptr);
/** Labels + flags for Game Settings / entryDetails. */
QVariantMap onlineFixOverlayInfo(const QString& installPath);

/**
 * Proton / Wine launch extras for SteamFix / Online-Fix overlays (SOFL-compatible).
 * Sets WINEDLLOVERRIDES, requests Steam Runtime, and when Steam is running attaches
 * LD_PRELOAD gameoverlayrenderer. Safe no-op when overlay files are missing/disabled.
 */
void applyOnlineFixLaunchInfo(const QString& installPath, LaunchInfo* info);

#if defined(Q_OS_LINUX)
bool isSteamClientRunning();
/** Best-effort: spawn `steam` detached. Returns true if the process was started. */
bool tryStartSteamClient();
#endif

} // namespace arachnel::core

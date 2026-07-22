#pragma once

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

} // namespace arachnel::core

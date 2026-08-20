#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

namespace arachnel::core {

struct WineErrorWatchHints {
    QString installPath;
    QString executableName;
    QString fakeSteamAppId;
};

/** Pids tied to this launch: process tree plus Wine/Proton kids reparented off the tree. */
QList<qint64> relatedLaunchPids(qint64 launchProcessId, const WineErrorWatchHints& hints);

/** True if a modal/error dialog from this launch is visible (Win32 MessageBox / Wine on X11). */
bool wineErrorDialogVisible(qint64 launchProcessId, const WineErrorWatchHints& hints = {});

/** True if the game exe (or Wine path to it) is still among related launch pids. */
bool relatedGameExecutableAlive(qint64 launchProcessId, const WineErrorWatchHints& hints);

} // namespace arachnel::core

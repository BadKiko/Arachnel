#pragma once

#include "library_model.h"
#include "plugin_interface.h"
#include "settings_store.h"

#include <QString>
#include <QStringList>

namespace arachnel::core {

// Direct game.exe target for OS / Steam shortcuts (not Proton-wrapped).
struct GameLaunchTarget {
    QString executable;
    QString workingDirectory;
    QStringList arguments;
    QString title;
};

GameLaunchTarget resolveGameLaunchTarget(const LibraryGame& game, const LaunchInfo& pluginInfo,
                                         const SettingsStore& settings);

QString joinLaunchArguments(const QStringList& args);
QString sanitizeShortcutFileName(const QString& title);

} // namespace arachnel::core

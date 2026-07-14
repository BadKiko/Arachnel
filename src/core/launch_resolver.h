#pragma once

#include "library_model.h"
#include "plugin_interface.h"
#include "settings_store.h"

#include <QProcessEnvironment>
#include <QStringList>

namespace arachnel::core {

struct ResolvedLaunch {
    QString program;
    QStringList arguments;
    QString workingDirectory;
    QProcessEnvironment environment;
};

QStringList splitLaunchArguments(const QString& text);

ResolvedLaunch resolveLaunch(const LaunchInfo& pluginInfo, const LibraryGame& game,
                             const SettingsStore& settings);

} // namespace arachnel::core

#pragma once

#include "plugin_interface.h"

#include <QString>

namespace freetp {

struct LinuxFixLaunchOptions {
    bool preferSteamOverlay = true;
    bool autoFakeSteamWhenSteamDown = true;
};

bool linuxFixLaunchEnabled();

void prepareLinuxFixInstall(const QString& installPath, const QString& pluginResourceRoot);

void applyLinuxFixLaunchInfo(const QString& installPath, const QString& pluginResourceRoot,
                             const LinuxFixLaunchOptions& options,
                             arachnel::core::LaunchInfo* info);

QString findBestGameExecutable(const QString& rootDir);

} // namespace freetp

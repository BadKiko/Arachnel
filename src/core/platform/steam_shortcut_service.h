#pragma once

#include "game_launch_target.h"

#include <QString>

namespace arachnel::core {

struct SteamShortcutRequest {
    GameLaunchTarget target;
    QString steamAppId;
    QString coverFileUrl;
    bool openVr = false;
};

struct SteamShortcutResult {
    bool ok = false;
    QString error;
    QString shortcutsPath;
    quint32 gridAppId = 0;
};

SteamShortcutResult addOrUpdateSteamShortcut(const SteamShortcutRequest& request);

QString findSteamInstallPath();
QString findSteamShortcutsVdfPath();

} // namespace arachnel::core

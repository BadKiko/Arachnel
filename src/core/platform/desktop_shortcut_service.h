#pragma once

#include "game_launch_target.h"

#include <QString>

namespace arachnel::core {

// Windows: .lnk via WScript. Linux: freedesktop .desktop. Other: unsupported.
bool createOsShortcut(const QString& linkPath, const GameLaunchTarget& target, QString* errorOut);

} // namespace arachnel::core

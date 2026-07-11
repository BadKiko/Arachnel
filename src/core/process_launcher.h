#pragma once

#include "plugin_interface.h"

#include <QString>

namespace arachnel::core {

class ProcessLauncher
{
public:
    static bool launch(const LaunchInfo& info, QString* errorOut = nullptr);
};

} // namespace arachnel::core

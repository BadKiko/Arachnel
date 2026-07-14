#pragma once

#include "launch_resolver.h"

#include <QString>

namespace arachnel::core {

class ProcessLauncher
{
public:
    static bool launch(const ResolvedLaunch& launch, QString* errorOut = nullptr,
                       qint64* processIdOut = nullptr);
};

} // namespace arachnel::core

#pragma once

#include "launch_resolver.h"

#include <QString>

namespace arachnel::core {

class ProcessLauncher
{
public:
    /**
     * Launch detached. When logFilePath is set, the child's stdout/stderr are
     * redirected there so Proton/Wine diagnostics are captured for the UI log.
     */
    static bool launch(const ResolvedLaunch& launch, QString* errorOut = nullptr,
                       qint64* processIdOut = nullptr, const QString& logFilePath = {});
};

} // namespace arachnel::core

#pragma once

#include <QtGlobal>

namespace arachnel::core {

class ProcessTracker
{
public:
    static bool isProcessRunning(qint64 processId);
    static bool terminateProcess(qint64 processId);
};

} // namespace arachnel::core

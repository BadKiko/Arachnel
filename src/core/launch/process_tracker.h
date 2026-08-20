#pragma once

#include <QList>
#include <QtGlobal>

namespace arachnel::core {

class ProcessTracker
{
public:
    static bool isProcessRunning(qint64 processId);
    static bool terminateProcess(qint64 processId);
    /** Root pid plus descendants / process-group peers (empty if rootPid <= 0). */
    static QList<qint64> processTreePids(qint64 processId);
};

} // namespace arachnel::core

#pragma once

#include <QList>
#include <QtGlobal>

namespace arachnel::core {

class ProcessTracker
{
public:
    static bool isProcessRunning(qint64 processId, int* exitCodeOut = nullptr);
    static bool terminateProcess(qint64 processId);
    /** Root pid plus descendants / process-group peers (empty if rootPid <= 0). */
    static QList<qint64> processTreePids(qint64 processId);
    /** Keep an OS process handle so exit codes still work after the pid goes stale. */
    static void adoptNativeHandle(qint64 processId, quintptr nativeHandle);
    static void release(qint64 processId);
};

} // namespace arachnel::core

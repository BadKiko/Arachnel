#include "process_tracker.h"

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace arachnel::core {

bool ProcessTracker::isProcessRunning(const qint64 processId)
{
    if (processId <= 0)
        return false;

#if defined(Q_OS_WIN)
    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                static_cast<DWORD>(processId));
    if (!handle)
        return false;

    DWORD exitCode = 0;
    const bool alive =
        GetExitCodeProcess(handle, &exitCode) != 0 && exitCode == STILL_ACTIVE;
    CloseHandle(handle);
    return alive;
#else
    return kill(static_cast<pid_t>(processId), 0) == 0;
#endif
}

bool ProcessTracker::terminateProcess(const qint64 processId)
{
    if (processId <= 0)
        return false;

#if defined(Q_OS_WIN)
    HANDLE handle =
        OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(processId));
    if (!handle)
        return false;

    const bool ok = TerminateProcess(handle, 0) != 0;
    CloseHandle(handle);
    return ok;
#else
    return kill(static_cast<pid_t>(processId), SIGTERM) == 0;
#endif
}

} // namespace arachnel::core

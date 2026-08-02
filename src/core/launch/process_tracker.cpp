#include "process_tracker.h"

#include <QProcess>
#include <QStringList>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#else
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace arachnel::core {

namespace {

#if defined(Q_OS_WIN)
void collectDescendants(DWORD rootPid, QList<DWORD>* out)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return;

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    QList<DWORD> frontier{rootPid};
    QList<DWORD> all;

    while (!frontier.isEmpty()) {
        const DWORD parent = frontier.takeFirst();
        if (!Process32FirstW(snap, &entry))
            break;
        do {
            if (entry.th32ParentProcessID != parent || entry.th32ProcessID == parent)
                continue;
            if (all.contains(entry.th32ProcessID))
                continue;
            all.append(entry.th32ProcessID);
            frontier.append(entry.th32ProcessID);
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    *out = all;
}

bool terminatePid(DWORD pid)
{
    HANDLE handle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!handle)
        return false;
    const bool ok = TerminateProcess(handle, 1) != 0;
    CloseHandle(handle);
    return ok;
}
#endif

} // namespace

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
    // Kill children first - many games are launched via a stub that stays alive
    // (or exits) while the real exe is a descendant.
    QList<DWORD> kids;
    collectDescendants(static_cast<DWORD>(processId), &kids);
    bool any = false;
    for (int i = kids.size() - 1; i >= 0; --i)
        any = terminatePid(kids.at(i)) || any;
    any = terminatePid(static_cast<DWORD>(processId)) || any;

    // Fallback if OpenProcess(PROCESS_TERMINATE) is denied on some children.
    if (isProcessRunning(processId) || !any) {
        QProcess killer;
        killer.start(QStringLiteral("taskkill"),
                     {QStringLiteral("/PID"), QString::number(processId), QStringLiteral("/T"),
                      QStringLiteral("/F")});
        killer.waitForFinished(8000);
        any = killer.exitCode() == 0 || !isProcessRunning(processId);
    }
    return any || !isProcessRunning(processId);
#else
    // Best-effort process group, then the pid itself.
    kill(-static_cast<pid_t>(processId), SIGTERM);
    if (kill(static_cast<pid_t>(processId), SIGTERM) == 0)
        return true;
    return kill(static_cast<pid_t>(processId), SIGKILL) == 0;
#endif
}

} // namespace arachnel::core

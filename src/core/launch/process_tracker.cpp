#include "process_tracker.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSet>
#include <QStringList>
#include <QThread>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#else
#include <errno.h>
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
#else

struct ProcIds {
    pid_t pid = 0;
    pid_t ppid = 0;
    pid_t pgrp = 0;
};

bool parseProcStat(pid_t pid, ProcIds* out)
{
    QFile file(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray line = file.readAll();
    const int lparen = line.indexOf('(');
    const int rparen = line.lastIndexOf(')');
    if (lparen < 0 || rparen < 0 || rparen + 2 >= line.size())
        return false;
    const QList<QByteArray> rest = line.mid(rparen + 2).split(' ');
    if (rest.size() < 3)
        return false;
    out->pid = pid;
    out->ppid = static_cast<pid_t>(rest.at(1).toLong());
    out->pgrp = static_cast<pid_t>(rest.at(2).toLong());
    return true;
}

QList<ProcIds> listProcIds()
{
    QList<ProcIds> out;
    const QStringList names =
        QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : names) {
        bool ok = false;
        const qint64 pid = name.toLongLong(&ok);
        if (!ok || pid <= 0)
            continue;
        ProcIds ids;
        if (parseProcStat(static_cast<pid_t>(pid), &ids))
            out.append(ids);
    }
    return out;
}

QList<qint64> collectTreePids(qint64 rootPid)
{
    QSet<qint64> seen;
    QList<qint64> frontier{rootPid};
    QList<qint64> all;
    const QList<ProcIds> snapshot = listProcIds();
    // Only walk a process group when the launched pid is the leader (setpgid(0,0)
    // in the child). Otherwise we'd pick up Arachnel and its other kids.
    pid_t group = 0;
    ProcIds rootIds;
    if (parseProcStat(static_cast<pid_t>(rootPid), &rootIds)) {
        if (rootIds.pgrp == rootIds.pid && rootIds.pgrp > 1)
            group = rootIds.pgrp;
    } else if (rootPid > 1) {
        // Parent already gone; Wine/Proton kids often keep pgid == old pid.
        group = static_cast<pid_t>(rootPid);
    }

    if (group > 1) {
        for (const ProcIds& ids : snapshot) {
            if (ids.pgrp == group && ids.pid != static_cast<pid_t>(rootPid))
                frontier.append(ids.pid);
        }
    }

    while (!frontier.isEmpty()) {
        const qint64 parent = frontier.takeFirst();
        if (seen.contains(parent))
            continue;
        seen.insert(parent);
        if (parent != rootPid)
            all.append(parent);
        for (const ProcIds& ids : snapshot) {
            if (ids.ppid == static_cast<pid_t>(parent) && !seen.contains(ids.pid))
                frontier.append(ids.pid);
        }
    }
    return all;
}

bool pidAlive(qint64 processId)
{
    return ::kill(static_cast<pid_t>(processId), 0) == 0 || errno == EPERM;
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
    if (pidAlive(processId))
        return true;
    return !collectTreePids(processId).isEmpty();
#endif
}

bool ProcessTracker::terminateProcess(const qint64 processId)
{
    if (processId <= 0)
        return false;

#if defined(Q_OS_WIN)
    QList<DWORD> kids;
    collectDescendants(static_cast<DWORD>(processId), &kids);
    bool any = false;
    for (int i = kids.size() - 1; i >= 0; --i)
        any = terminatePid(kids.at(i)) || any;
    any = terminatePid(static_cast<DWORD>(processId)) || any;

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
    QList<qint64> tree = collectTreePids(processId);
    tree.append(processId);

    ::kill(-static_cast<pid_t>(processId), SIGTERM);
    bool any = false;
    for (int i = tree.size() - 1; i >= 0; --i)
        any = (::kill(static_cast<pid_t>(tree.at(i)), SIGTERM) == 0) || any;

    QThread::msleep(400);

    ::kill(-static_cast<pid_t>(processId), SIGKILL);
    for (int i = tree.size() - 1; i >= 0; --i)
        ::kill(static_cast<pid_t>(tree.at(i)), SIGKILL);

    return any || !isProcessRunning(processId);
#endif
}

} // namespace arachnel::core

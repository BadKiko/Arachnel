#include "process_tracker.h"

#include <QDir>
#include <QFile>
#include <QHash>
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
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace arachnel::core {

namespace {

#if defined(Q_OS_WIN)
QHash<qint64, HANDLE> g_watchedHandles;

void closeWatched(qint64 processId)
{
    const auto it = g_watchedHandles.find(processId);
    if (it == g_watchedHandles.end())
        return;
    CloseHandle(it.value());
    g_watchedHandles.erase(it);
}

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

QList<qint64> ProcessTracker::processTreePids(const qint64 processId)
{
    QList<qint64> out;
    if (processId <= 0)
        return out;
#if defined(Q_OS_WIN)
    out.append(processId);
    QList<DWORD> kids;
    collectDescendants(static_cast<DWORD>(processId), &kids);
    for (DWORD kid : kids)
        out.append(static_cast<qint64>(kid));
#else
    if (pidAlive(processId))
        out.append(processId);
    out += collectTreePids(processId);
#endif
    return out;
}

bool ProcessTracker::isProcessRunning(const qint64 processId, int* exitCodeOut)
{
    if (exitCodeOut)
        *exitCodeOut = -1;
    if (processId <= 0)
        return false;

#if defined(Q_OS_WIN)
    HANDLE handle = g_watchedHandles.value(processId, nullptr);
    bool owned = handle != nullptr;
    if (!handle) {
        handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                             static_cast<DWORD>(processId));
        owned = false;
    }
    if (!handle)
        return false;

    DWORD exitCode = 0;
    if (GetExitCodeProcess(handle, &exitCode) == 0) {
        if (!owned)
            CloseHandle(handle);
        return false;
    }
    if (exitCode == STILL_ACTIVE) {
        if (!owned)
            CloseHandle(handle);
        return true;
    }
    if (exitCodeOut)
        *exitCodeOut = static_cast<int>(exitCode);
    if (owned)
        closeWatched(processId);
    else
        CloseHandle(handle);
    return false;
#else
    if (pidAlive(processId))
        return true;
    if (!collectTreePids(processId).isEmpty())
        return true;
    int status = 0;
    const pid_t waited = ::waitpid(static_cast<pid_t>(processId), &status, WNOHANG);
    if (waited == processId && exitCodeOut) {
        if (WIFEXITED(status))
            *exitCodeOut = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            *exitCodeOut = 128 + WTERMSIG(status);
    }
    return false;
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

void ProcessTracker::adoptNativeHandle(qint64 processId, quintptr nativeHandle)
{
#if defined(Q_OS_WIN)
    if (processId <= 0)
        return;
    closeWatched(processId);
    HANDLE handle = reinterpret_cast<HANDLE>(nativeHandle);
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE,
                             static_cast<DWORD>(processId));
    }
    if (handle && handle != INVALID_HANDLE_VALUE)
        g_watchedHandles.insert(processId, handle);
#else
    Q_UNUSED(processId);
    Q_UNUSED(nativeHandle);
#endif
}

void ProcessTracker::release(qint64 processId)
{
#if defined(Q_OS_WIN)
    closeWatched(processId);
#else
    Q_UNUSED(processId);
#endif
}

} // namespace arachnel::core

#include "process_launcher.h"

#include "crash_log.h"
#include "process_tracker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSet>

#include <string>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#elif defined(Q_OS_UNIX)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace arachnel::core {
namespace {

#if defined(Q_OS_WIN)

QString quoteWindowsArg(const QString& text)
{
    if (text.isEmpty())
        return QStringLiteral("\"\"");
    if (!text.contains(QLatin1Char(' ')) && !text.contains(QLatin1Char('\t'))
        && !text.contains(QLatin1Char('"')))
        return text;

    QString escaped;
    escaped.reserve(text.size() + 4);
    escaped += QLatin1Char('"');
    int backslashes = 0;
    for (const QChar ch : text) {
        if (ch == QLatin1Char('\\')) {
            ++backslashes;
            continue;
        }
        if (ch == QLatin1Char('"')) {
            escaped += QString(backslashes * 2 + 1, QLatin1Char('\\'));
            backslashes = 0;
            escaped += QLatin1Char('"');
            continue;
        }
        if (backslashes > 0) {
            escaped += QString(backslashes, QLatin1Char('\\'));
            backslashes = 0;
        }
        escaped += ch;
    }
    if (backslashes > 0)
        escaped += QString(backslashes * 2, QLatin1Char('\\'));
    escaped += QLatin1Char('"');
    return escaped;
}

void setLaunchFailed(QString* errorOut, DWORD winError)
{
    if (!errorOut)
        return;
    *errorOut = QCoreApplication::translate("Core", "Failed to start process")
                + QStringLiteral(" (Win32 %1)").arg(winError);
}

bool adoptWindowsProcess(HANDLE process, qint64* processIdOut)
{
    if (!process || process == INVALID_HANDLE_VALUE)
        return false;
    const DWORD pid = GetProcessId(process);
    if (pid == 0) {
        CloseHandle(process);
        return false;
    }
    ProcessTracker::adoptNativeHandle(static_cast<qint64>(pid),
                                      reinterpret_cast<quintptr>(process));
    if (processIdOut)
        *processIdOut = static_cast<qint64>(pid);
    return true;
}

QSet<qint64> processIdsNamed(const QString& exeName)
{
    QSet<qint64> ids;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return ids;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snap, &entry)) {
        do {
            const QString name = QString::fromWCharArray(entry.szExeFile);
            if (name.compare(exeName, Qt::CaseInsensitive) == 0)
                ids.insert(static_cast<qint64>(entry.th32ProcessID));
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return ids;
}

bool adoptPid(qint64 pid, qint64* processIdOut)
{
    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE,
                                static_cast<DWORD>(pid));
    if (!handle)
        return false;
    return adoptWindowsProcess(handle, processIdOut);
}

bool waitForNewExecutablePid(const QString& exeName, const QString& nativeProgram,
                             const QSet<qint64>& before, qint64* processIdOut)
{
    for (int i = 0; i < 15; ++i) {
        Sleep(40);
        const QSet<qint64> born = processIdsNamed(exeName) - before;
        if (born.isEmpty())
            continue;
        for (qint64 candidate : born) {
            HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE,
                                        static_cast<DWORD>(candidate));
            if (!handle)
                continue;
            wchar_t path[MAX_PATH * 4] = {};
            DWORD n = MAX_PATH * 4;
            if (QueryFullProcessImageNameW(handle, 0, path, &n)) {
                const QString image = QDir::toNativeSeparators(QString::fromWCharArray(path));
                if (image.compare(nativeProgram, Qt::CaseInsensitive) == 0) {
                    arachnel::logDiagnostic(
                        QStringLiteral("Windows detached game pid=%1").arg(candidate));
                    return adoptWindowsProcess(handle, processIdOut);
                }
            }
            CloseHandle(handle);
        }
        const qint64 pid = *born.constBegin();
        arachnel::logDiagnostic(QStringLiteral("Windows detached game pid=%1").arg(pid));
        return adoptPid(pid, processIdOut);
    }
    return false;
}

bool launchWindowsCreateProcess(const QString& nativeProgram, const QString& nativeWorkDir,
                                const QStringList& arguments, QString* errorOut,
                                qint64* processIdOut)
{
    QString command = quoteWindowsArg(nativeProgram);
    for (const QString& argument : arguments)
        command += QLatin1Char(' ') + quoteWindowsArg(argument);
    std::wstring commandW = command.toStdWString();

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_SHOWNORMAL;

    PROCESS_INFORMATION info{};
    // Inherit the parent env. A rebuilt env block + CREATE_NEW_PROCESS_GROUP
    // made SteamFix / Super Meat Boy ExitProcess immediately.
    DWORD flags = CREATE_UNICODE_ENVIRONMENT | CREATE_BREAKAWAY_FROM_JOB;
    BOOL ok = CreateProcessW(reinterpret_cast<LPCWSTR>(nativeProgram.utf16()), commandW.data(),
                             nullptr, nullptr, FALSE, flags, nullptr,
                             nativeWorkDir.isEmpty()
                                 ? nullptr
                                 : reinterpret_cast<LPCWSTR>(nativeWorkDir.utf16()),
                             &startup, &info);
    if (!ok) {
        flags = CREATE_UNICODE_ENVIRONMENT;
        ok = CreateProcessW(reinterpret_cast<LPCWSTR>(nativeProgram.utf16()), commandW.data(),
                            nullptr, nullptr, FALSE, flags, nullptr,
                            nativeWorkDir.isEmpty()
                                ? nullptr
                                : reinterpret_cast<LPCWSTR>(nativeWorkDir.utf16()),
                            &startup, &info);
    }
    if (!ok) {
        setLaunchFailed(errorOut, GetLastError());
        return false;
    }
    CloseHandle(info.hThread);
    arachnel::logDiagnostic(
        QStringLiteral("Windows CreateProcess pid=%1").arg(info.dwProcessId));
    return adoptWindowsProcess(info.hProcess, processIdOut);
}

bool launchWindowsDetached(const ResolvedLaunch& launch, const QString& workDir, QString* errorOut,
                           qint64* processIdOut)
{
    const QString nativeProgram = QDir::toNativeSeparators(launch.program);
    const QString nativeWorkDir = QDir::toNativeSeparators(workDir);
    const QString exeName = QFileInfo(nativeProgram).fileName();
    const QSet<qint64> before = processIdsNamed(exeName);

    // Detach via `cmd /c start` so the game is not a child of the Qt process.
    // SteamFix / Super Meat Boy exit immediately when spawned from Arachnel
    // (inherited DLL directory / job). System32\cmd.exe avoids the FreeTP stub
    // planted next to the game.
    wchar_t sysDir[MAX_PATH] = {};
    GetSystemDirectoryW(sysDir, MAX_PATH);
    const QString cmdPath =
        QDir::toNativeSeparators(QString::fromWCharArray(sysDir) + QStringLiteral("\\cmd.exe"));
    QString startArgs = QStringLiteral("/c start \"\" /D %1 %2")
                            .arg(quoteWindowsArg(nativeWorkDir), quoteWindowsArg(nativeProgram));
    for (const QString& argument : launch.arguments)
        startArgs += QLatin1Char(' ') + quoteWindowsArg(argument);

    std::wstring cmdW = cmdPath.toStdWString();
    std::wstring argsW = startArgs.toStdWString();
    std::wstring dirW = nativeWorkDir.toStdWString();

    SHELLEXECUTEINFOW exec{};
    exec.cbSize = sizeof(exec);
    exec.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOZONECHECKS;
    exec.lpFile = cmdW.c_str();
    exec.lpParameters = argsW.c_str();
    exec.lpDirectory = dirW.empty() ? nullptr : dirW.c_str();
    exec.nShow = SW_HIDE;

    if (ShellExecuteExW(&exec)) {
        if (exec.hProcess)
            CloseHandle(exec.hProcess);
        arachnel::logDiagnostic(QStringLiteral("Windows cmd start %1").arg(nativeProgram));
        if (waitForNewExecutablePid(exeName, nativeProgram, before, processIdOut))
            return true;
        // Single-instance games reuse the already-running exe.
        if (!before.isEmpty() && adoptPid(*before.constBegin(), processIdOut)) {
            arachnel::logDiagnostic(
                QStringLiteral("Windows cmd start reused pid=%1").arg(*before.constBegin()));
            return true;
        }
        arachnel::logDiagnostic(QStringLiteral("Windows cmd start: game pid not seen yet"));
        if (processIdOut)
            *processIdOut = 0;
        return true;
    }

    arachnel::logDiagnostic(
        QStringLiteral("Windows cmd start failed (Win32 %1), trying ShellExecute")
            .arg(GetLastError()));

    QString params;
    for (const QString& argument : launch.arguments) {
        if (!params.isEmpty())
            params += QLatin1Char(' ');
        params += quoteWindowsArg(argument);
    }
    std::wstring fileW = nativeProgram.toStdWString();
    std::wstring paramsW = params.toStdWString();

    SHELLEXECUTEINFOW exeExec{};
    exeExec.cbSize = sizeof(exeExec);
    exeExec.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOZONECHECKS;
    exeExec.lpFile = fileW.c_str();
    exeExec.lpParameters = paramsW.empty() ? nullptr : paramsW.c_str();
    exeExec.lpDirectory = dirW.empty() ? nullptr : dirW.c_str();
    exeExec.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&exeExec) && exeExec.hProcess) {
        arachnel::logDiagnostic(
            QStringLiteral("Windows ShellExecute pid=%1").arg(GetProcessId(exeExec.hProcess)));
        if (adoptWindowsProcess(exeExec.hProcess, processIdOut))
            return true;
    }

    return launchWindowsCreateProcess(nativeProgram, nativeWorkDir, launch.arguments, errorOut,
                                      processIdOut);
}

#endif

} // namespace

bool ProcessLauncher::launch(const ResolvedLaunch& launch, QString* errorOut, qint64* processIdOut,
                             const QString& logFilePath)
{
    if (launch.program.isEmpty()) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "Executable is not set");
        return false;
    }

    QFileInfo programInfo(launch.program);
    if (!programInfo.exists()) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("Core", "File not found: %1").arg(launch.program);
        return false;
    }

    QString workDir = launch.workingDirectory;
    if (workDir.isEmpty())
        workDir = programInfo.absolutePath();

    // Some engines (e.g. Tommunism / Super Meat Boy) open UserData/*.log without mkdir.
    QDir().mkpath(workDir + QStringLiteral("/UserData"));

#if defined(Q_OS_WIN)
    Q_UNUSED(logFilePath);
    return launchWindowsDetached(launch, workDir, errorOut, processIdOut);
#else
    QProcess process;
    process.setProgram(launch.program);
    process.setArguments(launch.arguments);
    process.setWorkingDirectory(workDir);
    process.setProcessEnvironment(launch.environment);

    const QString capturePath = logFilePath.trimmed();
    const QByteArray captureBytes = capturePath.toUtf8();
    process.setChildProcessModifier([captureBytes]() {
        ::setpgid(0, 0);
        if (!captureBytes.isEmpty()) {
            const int fd = ::open(captureBytes.constData(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) {
                ::dup2(fd, STDOUT_FILENO);
                ::dup2(fd, STDERR_FILENO);
                if (fd > STDERR_FILENO)
                    ::close(fd);
            }
        }
    });

    qint64 processId = 0;
    const bool ok = process.startDetached(&processId);
    if (!ok && errorOut)
        *errorOut = QCoreApplication::translate("Core", "Failed to start process");
    if (ok && processIdOut)
        *processIdOut = processId;
    return ok;
#endif
}

} // namespace arachnel::core

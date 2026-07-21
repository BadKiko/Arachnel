#include "crash_log.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QUrl>

#include <cstdio>
#include <vector>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#else
#include <csignal>
#include <cstring>
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <unistd.h>
#endif

namespace arachnel {

#include "crash_log_internal.h"

void markApplicationShuttingDown()
{
    g_shuttingDown = true;
}

bool isCrashDialogMode(int argc, char* argv[])
{
    if (!argv)
        return false;
    for (int i = 0; argv[i]; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--crash-dialog"))
            return true;
    }
    return false;
}

void installCrashLogging()
{
#if defined(Q_OS_WIN)
    attachParentConsole();
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
#else
    struct sigaction action = {};
    action.sa_sigaction = linuxSignalHandler;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&action.sa_mask);

    const int crashSignals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS};
    for (const int sigNum : crashSignals)
        sigaction(sigNum, &action, nullptr);
#endif
    qInstallMessageHandler(qtMessageHandler);
    QDir().mkpath(logDirectory());
}

void logRunStarted(int argc, char* argv[])
{
    g_isCrashDialogProcess = isCrashDialogMode(argc, argv);

    QStringList args;
    if (argv) {
        for (int i = 0; argv[i]; ++i)
            args.append(QString::fromLocal8Bit(argv[i]));
    }

    if (!args.isEmpty()) {
        g_runExePath = args.constFirst();
        g_runArgsLine = args.mid(1).join(QLatin1Char(' '));
    } else {
        g_runExePath = QCoreApplication::applicationFilePath();
    }

    if (g_isCrashDialogProcess)
        return;

    const QString header = QStringLiteral("=== Arachnel %1 started %2 ===")
                               .arg(QCoreApplication::applicationVersion(),
                                    QDateTime::currentDateTime().toString(Qt::ISODate));
    writeLine(header);
    if (!args.isEmpty())
        writeLine(QStringLiteral("Args: %1").arg(args.join(QLatin1Char(' '))));
}

void logRunFinished(int exitCode)
{
    if (g_isCrashDialogProcess)
        return;

    const QString footer = QStringLiteral("=== Arachnel exited with code %1 at %2 ===")
                               .arg(exitCode)
                               .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    writeLine(footer);

    if (exitCode != 0) {
        const QString summary =
            QStringLiteral("[%1] Abnormal exit: code %2")
                .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
                .arg(exitCode);
        QMutexLocker lock(&g_logMutex);
        appendToFile(crashLogPath(), summary);
    }
}

void logDiagnostic(const QString& line)
{
    writeLine(QStringLiteral("[diag] %1").arg(line));
}

void logQmlWarning(const QUrl& url, int line, int column, const QString& description)
{
    const QString location = url.isValid() ? url.toString(QUrl::RemoveQuery | QUrl::RemoveFragment)
                                           : QStringLiteral("(unknown QML file)");
    writeLine(QStringLiteral("[QML] %1:%2:%3: %4").arg(location).arg(line).arg(column).arg(description));
}

} // namespace arachnel

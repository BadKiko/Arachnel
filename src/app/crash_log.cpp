#include "crash_log.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QUrl>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <exception>
#include <thread>
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

namespace {

std::atomic_bool g_watchdogStarted{false};
std::atomic<qint64> g_mainPingSerial{0};
std::atomic<qint64> g_mainPingAck{0};

void cppTerminateHandler()
{
    const QString summary = QStringLiteral("Unhandled C++ exception (std::terminate)");
    QString extra = QStringLiteral("An exception escaped without being caught.");
    try {
        if (const std::exception_ptr ep = std::current_exception())
            std::rethrow_exception(ep);
    } catch (const std::exception& ex) {
        extra += QStringLiteral("\nwhat(): %1").arg(QString::fromLocal8Bit(ex.what()));
    } catch (...) {
        extra += QStringLiteral("\n(non-std exception)");
    }
#if defined(Q_OS_WIN)
    handleCrashReport(buildCrashReport(summary, extra, captureStackTraceWindows(nullptr)));
#else
    handleCrashReport(buildCrashReport(summary, extra, captureStackTraceUnix()));
#endif
    std::abort();
}

void startHangWatchdog()
{
    if (g_watchdogStarted.exchange(true))
        return;

    std::thread([]() {
        using namespace std::chrono_literals;
        constexpr int kHangSeconds = 25;
        while (!g_shuttingDown) {
            std::this_thread::sleep_for(3s);
            if (g_shuttingDown || g_isCrashDialogProcess)
                continue;
            QCoreApplication* app = QCoreApplication::instance();
            if (!app)
                continue;

            const qint64 ping = g_mainPingSerial.fetch_add(1) + 1;
            const bool queued = QMetaObject::invokeMethod(
                app,
                [ping]() { g_mainPingAck.store(ping, std::memory_order_release); },
                Qt::QueuedConnection);
            if (!queued)
                continue;

            for (int i = 0; i < kHangSeconds * 2 && !g_shuttingDown; ++i)
                std::this_thread::sleep_for(500ms);

            if (g_shuttingDown)
                break;
            if (g_mainPingAck.load(std::memory_order_acquire) == ping)
                continue;

            reportUiHang(kHangSeconds);

            while (!g_shuttingDown
                   && g_mainPingAck.load(std::memory_order_acquire)
                          != g_mainPingSerial.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(2s);
            }
        }
    }).detach();
}

} // namespace

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
    std::set_terminate(cppTerminateHandler);
    qInstallMessageHandler(qtMessageHandler);
    QDir().mkpath(logDirectory());
}

void logRunStarted(int argc, char* argv[])
{
    g_isCrashDialogProcess = isCrashDialogMode(argc, argv);
    g_appStartMs = QDateTime::currentMSecsSinceEpoch();
#if defined(Q_OS_WIN)
    g_mainThreadId = GetCurrentThreadId();
#endif

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

    startHangWatchdog();
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

void logBreadcrumb(const QString& where, const QString& detail)
{
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    QString line = QStringLiteral("[%1] %2").arg(stamp, where);
    if (!detail.isEmpty())
        line += QStringLiteral(": %1").arg(detail);
    {
        QMutexLocker lock(&g_logMutex);
        rememberBreadcrumb(line);
        appendToFile(runLogPath(), QStringLiteral("[crumb] %1").arg(line));
        rememberRecentLine(QStringLiteral("[crumb] %1").arg(line));
    }
}

void logQmlWarning(const QUrl& url, int line, int column, const QString& description)
{
    const QString location = url.isValid() ? url.toString(QUrl::RemoveQuery | QUrl::RemoveFragment)
                                           : QStringLiteral("(unknown QML file)");
    writeLine(QStringLiteral("[QML] %1:%2:%3: %4").arg(location).arg(line).arg(column).arg(description));
}

} // namespace arachnel

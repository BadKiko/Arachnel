#include "crash_log.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QStandardPaths>
#include <QTextStream>

#include <cstdio>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace arachnel {
namespace {

QMutex g_logMutex;
QString g_logDir;

QString logDirectory()
{
    if (!g_logDir.isEmpty())
        return g_logDir;

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    g_logDir = dir;
    return g_logDir;
}

QString runLogPath()
{
    return logDirectory() + QStringLiteral("/run.log");
}

QString crashLogPath()
{
    return logDirectory() + QStringLiteral("/crash.log");
}

void appendToFile(const QString& path, const QString& text)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;

    QTextStream stream(&file);
    stream << text;
    if (!text.endsWith(QLatin1Char('\n')))
        stream << QLatin1Char('\n');
}

void writeLine(const QString& line, bool toStderr = true)
{
    QMutexLocker lock(&g_logMutex);
    appendToFile(runLogPath(), line);
    if (toStderr) {
        fprintf(stderr, "%s\n", qPrintable(line));
        fflush(stderr);
    }
}

void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    Q_UNUSED(context);

    const char* level = "LOG";
    switch (type) {
    case QtDebugMsg:
        level = "DEBUG";
        break;
    case QtInfoMsg:
        level = "INFO";
        break;
    case QtWarningMsg:
        level = "WARN";
        break;
    case QtCriticalMsg:
        level = "CRITICAL";
        break;
    case QtFatalMsg:
        level = "FATAL";
        break;
    }

    const QString line =
        QStringLiteral("[%1] %2: %3")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODate), QString::fromLatin1(level), msg);
    writeLine(line, type != QtDebugMsg);

    if (type == QtFatalMsg)
        abort();
}

#if defined(Q_OS_WIN)
bool attachParentConsole()
{
    if (!qEnvironmentVariableIsSet("ARACHNEL_DEV_RUN"))
        return false;
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return false;

    FILE* dummy = nullptr;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);
    freopen_s(&dummy, "CONIN$", "r", stdin);
    return true;
}

QString describeExceptionCode(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        return QStringLiteral("Access violation");
    case EXCEPTION_STACK_OVERFLOW:
        return QStringLiteral("Stack overflow");
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return QStringLiteral("Integer divide by zero");
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return QStringLiteral("Illegal instruction");
    default:
        return QStringLiteral("Windows exception 0x%1").arg(code, 8, 16, QLatin1Char('0'));
    }
}

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* info)
{
    if (!info || !info->ExceptionRecord)
        return EXCEPTION_CONTINUE_SEARCH;

    const DWORD code = info->ExceptionRecord->ExceptionCode;
    const QString summary = describeExceptionCode(code);
    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString body = QStringLiteral("[%1] CRASH: %2 (code 0x%3)")
                             .arg(timestamp, summary)
                             .arg(code, 8, 16, QLatin1Char('0'));

    {
        QMutexLocker lock(&g_logMutex);
        appendToFile(runLogPath(), body);
        appendToFile(crashLogPath(), body);
    }

    fprintf(stderr, "\n%s\n", qPrintable(body));
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

} // namespace

void installCrashLogging()
{
#if defined(Q_OS_WIN)
    attachParentConsole();
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
#endif
    qInstallMessageHandler(qtMessageHandler);
    QDir().mkpath(logDirectory());
}

void logRunStarted(int argc, char* argv[])
{
    Q_UNUSED(argc);

    QStringList args;
    if (argv) {
        for (int i = 0; argv[i]; ++i)
            args.append(QString::fromLocal8Bit(argv[i]));
    }

    const QString header = QStringLiteral("=== Arachnel %1 started %2 ===")
                               .arg(QCoreApplication::applicationVersion(),
                                    QDateTime::currentDateTime().toString(Qt::ISODate));
    writeLine(header);
    if (!args.isEmpty())
        writeLine(QStringLiteral("Args: %1").arg(args.join(QLatin1Char(' '))));
}

void logRunFinished(int exitCode)
{
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

} // namespace arachnel

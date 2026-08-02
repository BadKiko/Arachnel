#include "crash_log_internal.h"

#include <QCoreApplication>
#include <QDateTime>
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

QMutex g_logMutex;
QString g_logDir;
QString g_runExePath;
QString g_runArgsLine;
bool g_isCrashDialogProcess = false;
bool g_shuttingDown = false;
QStringList g_recentLines;
QStringList g_breadcrumbs;
qint64 g_appStartMs = 0;
#if defined(Q_OS_WIN)
DWORD g_mainThreadId = 0;
#endif

constexpr int kRecentLogLines = 200;
constexpr int kBreadcrumbLines = 48;

constexpr const char* kGithubIssuesNew =
    "https://github.com/BadKiko/Arachnel/issues/new";

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

QString latestCrashReportPath()
{
    return logDirectory() + QStringLiteral("/crash-report-latest.txt");
}

QString pendingCrashMarkerPath()
{
    return logDirectory() + QStringLiteral("/crash-pending.json");
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

void writeTextFile(const QString& path, const QString& text)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;

    QTextStream stream(&file);
    stream << text;
}

QString readTextFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

QString readPendingField(const QString& key)
{
    const QString raw = readTextFile(pendingCrashMarkerPath());
    if (raw.isEmpty())
        return {};

    const QJsonObject obj = QJsonDocument::fromJson(raw.toUtf8()).object();
    return obj.value(key).toString();
}

void removePendingMarker()
{
    QFile::remove(pendingCrashMarkerPath());
}

QString latestCrashDumpPath()
{
    return logDirectory() + QStringLiteral("/crash-latest.dmp");
}

void rememberRecentLine(const QString& line)
{
    g_recentLines.append(line);
    while (g_recentLines.size() > kRecentLogLines)
        g_recentLines.removeFirst();
}

void rememberBreadcrumb(const QString& line)
{
    g_breadcrumbs.append(line);
    while (g_breadcrumbs.size() > kBreadcrumbLines)
        g_breadcrumbs.removeFirst();
}

void writeLine(const QString& line, bool toStderr)
{
    QMutexLocker lock(&g_logMutex);
    appendToFile(runLogPath(), line);
    rememberRecentLine(line);
    if (toStderr) {
        fprintf(stderr, "%s\n", qPrintable(line));
        fflush(stderr);
    }
}

QString percentEncode(const QString& value)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(value));
}

QString buildIssueUrl(const QString& summary, const QString& reportBody)
{
    const QString title = QStringLiteral("Crash: %1").arg(summary);
    const QString body = QStringLiteral(
                             "Auto-generated crash report.\n\n"
                             "Please describe what you were doing before the crash.\n\n"
                             "---\n\n%1")
                             .arg(reportBody);
    return QStringLiteral("%1?title=%2&body=%3")
        .arg(QLatin1String(kGithubIssuesNew), percentEncode(title), percentEncode(body));
}

void persistPendingCrash(const CrashReportData& report)
{
    writeTextFile(latestCrashReportPath(), report.details);

    QJsonObject obj;
    obj.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(Qt::ISODate));
    obj.insert(QStringLiteral("summary"), report.summary);
    obj.insert(QStringLiteral("reportPath"), latestCrashReportPath());
    obj.insert(QStringLiteral("issueUrl"), report.issueUrl);
    writeTextFile(pendingCrashMarkerPath(),
                  QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void logCrashLines(const CrashReportData& report)
{
    const QString stamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString headline =
        QStringLiteral("[%1] CRASH: %2").arg(stamp, report.summary);

    QMutexLocker lock(&g_logMutex);
    appendToFile(runLogPath(), headline);
    appendToFile(crashLogPath(), headline);
    appendToFile(runLogPath(), report.details);
    appendToFile(crashLogPath(), report.details);
}

CrashReportData buildCrashReport(const QString& summary, const QString& extraDetails,
                                 const QString& stackTrace)
{
    CrashReportData report;
    report.summary = summary;

    const qint64 uptimeSec =
        g_appStartMs > 0
            ? qMax<qint64>(0, (QDateTime::currentMSecsSinceEpoch() - g_appStartMs) / 1000)
            : 0;

    QStringList body;
    body.append(QStringLiteral("Arachnel %1").arg(QCoreApplication::applicationVersion()));
    body.append(QStringLiteral("Qt %1").arg(QString::fromLatin1(qVersion())));
    body.append(QStringLiteral("Time: %1")
                    .arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
    if (uptimeSec > 0)
        body.append(QStringLiteral("Uptime: %1s").arg(uptimeSec));
    body.append(QStringLiteral("Executable: %1").arg(g_runExePath));
    if (!g_runArgsLine.isEmpty())
        body.append(QStringLiteral("Args: %1").arg(g_runArgsLine));
    body.append(QStringLiteral("OS: %1 (%2)")
                    .arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture()));
    body.append(QStringLiteral("Summary: %1").arg(summary));
    if (!extraDetails.isEmpty())
        body.append(extraDetails);
    if (!stackTrace.isEmpty())
        body.append(stackTrace);

    if (!g_breadcrumbs.isEmpty()) {
        body.append(QStringLiteral("Breadcrumbs (%1):").arg(g_breadcrumbs.size()));
        body.append(g_breadcrumbs.join(QStringLiteral("\n")));
    }

    if (!g_recentLines.isEmpty()) {
        body.append(QStringLiteral("Recent log (%1 lines):").arg(g_recentLines.size()));
        body.append(g_recentLines.join(QStringLiteral("\n")));
    }

    if (summary.contains(QStringLiteral("Access violation"))
        && extraDetails.contains(QStringLiteral("0x0000000000000000"))) {
        body.append(QStringLiteral(
            "Hint: null pointer access. Check the fault module + last breadcrumbs "
            "(plugin DLL mismatch after app update is a common cause)."));
    } else if (summary.contains(QStringLiteral("Access violation"))
               && extraDetails.contains(QStringLiteral("0x0000000000000001"))) {
        body.append(QStringLiteral(
            "Hint: near-null pointer. Rebuild/redeploy source plugins if this followed an update."));
    } else if (summary.contains(QStringLiteral("not responding"), Qt::CaseInsensitive)
               || summary.contains(QStringLiteral("UI hang"), Qt::CaseInsensitive)) {
        body.append(QStringLiteral(
            "Hint: main thread blocked (catalog parse/merge, plugin call, or sync I/O). "
            "See breadcrumbs and the hung-thread stack."));
    }

    if (QFileInfo::exists(latestCrashDumpPath()))
        body.append(QStringLiteral("Minidump: %1").arg(latestCrashDumpPath()));
    body.append(QStringLiteral("Report file: %1").arg(latestCrashReportPath()));
    body.append(QStringLiteral("Run log: %1").arg(runLogPath()));

    report.details = body.join(QStringLiteral("\n"));
    report.issueUrl = buildIssueUrl(summary, report.details);
    return report;
}

void spawnCrashDialogUi()
{
    if (g_isCrashDialogProcess)
        return;

    QString exe = g_runExePath;
    if (exe.isEmpty())
        exe = QCoreApplication::applicationFilePath();

#if defined(Q_OS_WIN)
    QString commandLine = QStringLiteral("\"%1\" --crash-dialog").arg(exe);
    std::vector<wchar_t> commandLineBuffer(static_cast<size_t>(commandLine.size() + 1));
    const qsizetype commandLength = commandLine.size();
    if (commandLength > 0)
        commandLine.toWCharArray(commandLineBuffer.data());
    commandLineBuffer[static_cast<size_t>(commandLength)] = L'\0';
    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo = {};
    if (CreateProcessW(nullptr, commandLineBuffer.data(), nullptr, nullptr, FALSE,
                       DETACHED_PROCESS | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr,
                       &startupInfo, &processInfo)) {
        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);
    }
#else
    const QByteArray exeBytes = QFile::encodeName(exe);
    const pid_t child = fork();
    if (child == 0) {
        execl(exeBytes.constData(), exeBytes.constData(), "--crash-dialog", nullptr);
        _exit(1);
    }
#endif
}

void handleCrashReport(const CrashReportData& report)
{
    logCrashLines(report);

    fprintf(stderr, "\n%s\n\n%s\n", qPrintable(report.summary), qPrintable(report.details));
    fflush(stderr);

    if (g_shuttingDown || g_isCrashDialogProcess)
        return;

    persistPendingCrash(report);
    spawnCrashDialogUi();
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
    case 0xC0000374:
        return QStringLiteral("Heap corruption");
    default:
        return QStringLiteral("Windows exception 0x%1").arg(code, 8, 16, QLatin1Char('0'));
    }
}

QString formatAccessViolationDetails(const EXCEPTION_RECORD* record)
{
    if (!record || record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION
        || record->NumberParameters < 2) {
        return {};
    }

    const ULONG_PTR access = record->ExceptionInformation[0];
    const ULONG_PTR address = record->ExceptionInformation[1];
    QString accessType = QStringLiteral("access");
    if (access == 0)
        accessType = QStringLiteral("read");
    else if (access == 1)
        accessType = QStringLiteral("write");
    else if (access == 8)
        accessType = QStringLiteral("execute");
    return QStringLiteral("Invalid %1 at address 0x%2")
        .arg(accessType)
        .arg(address, QT_POINTER_SIZE * 2, 16, QLatin1Char('0'));
}

QString moduleForAddress(DWORD64 address)
{
    if (address == 0)
        return QStringLiteral("null pointer");

    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(address), &module)
        || !module) {
        return QStringLiteral("unknown module");
    }

    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
    if (length == 0)
        return QStringLiteral("unknown module");
    return QDir::fromNativeSeparators(QString::fromWCharArray(path, length));
}

void ensureSymbolEngine(HANDLE process)
{
    static bool initialized = false;
    if (initialized)
        return;
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES
                  | SYMOPT_FAIL_CRITICAL_ERRORS);
    SymInitialize(process, nullptr, TRUE);
    initialized = true;
}

void appendSymbolLine(QStringList& lines, HANDLE process, DWORD64 address)
{
    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)] = {};
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    DWORD64 displacement = 0;
    QString line = QStringLiteral("  0x%1").arg(address, 16, 16, QLatin1Char('0'));

    if (SymFromAddr(process, address, &displacement, symbol)) {
        line += QStringLiteral(" %1").arg(QString::fromLocal8Bit(symbol->Name));
        if (displacement > 0)
            line += QStringLiteral("+0x%1").arg(displacement, 0, 16);
    } else {
        line += QStringLiteral(" (symbol unavailable)");
    }

    DWORD lineDisplacement = 0;
    IMAGEHLP_LINE64 lineInfo = {};
    lineInfo.SizeOfStruct = sizeof(lineInfo);
    if (SymGetLineFromAddr64(process, address, &lineDisplacement, &lineInfo)) {
        const QString file = QDir::fromNativeSeparators(
            QString::fromLocal8Bit(lineInfo.FileName));
        line += QStringLiteral(" at %1:%2").arg(file).arg(lineInfo.LineNumber);
    }

    IMAGEHLP_MODULE64 moduleInfo = {};
    moduleInfo.SizeOfStruct = sizeof(moduleInfo);
    if (SymGetModuleInfo64(process, address, &moduleInfo))
        line += QStringLiteral(" [%1]").arg(QString::fromLocal8Bit(moduleInfo.ModuleName));

    lines.append(line);
}

QString captureStackTraceWindows(CONTEXT* optionalContext)
{
    HANDLE process = GetCurrentProcess();
    ensureSymbolEngine(process);

    QStringList lines;
    lines.append(QStringLiteral("Stack trace:"));

    bool captured = false;
    if (optionalContext) {
        STACKFRAME64 frame = {};
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Mode = AddrModeFlat;
#if defined(_M_X64) || defined(__x86_64__)
        const DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
        frame.AddrPC.Offset = optionalContext->Rip;
        frame.AddrFrame.Offset = optionalContext->Rbp;
        frame.AddrStack.Offset = optionalContext->Rsp;
#elif defined(_M_IX86)
        const DWORD machineType = IMAGE_FILE_MACHINE_I386;
        frame.AddrPC.Offset = optionalContext->Eip;
        frame.AddrFrame.Offset = optionalContext->Ebp;
        frame.AddrStack.Offset = optionalContext->Esp;
#else
        lines.append(QStringLiteral("  (stack walk unavailable on this CPU architecture)"));
        return lines.join(QLatin1Char('\n'));
#endif
        for (int frameIndex = 0; frameIndex < 64; ++frameIndex) {
            if (!StackWalk64(machineType, process, GetCurrentThread(), &frame, optionalContext,
                             nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
                break;
            }
            if (frame.AddrPC.Offset == 0)
                break;
            appendSymbolLine(lines, process, frame.AddrPC.Offset);
            captured = true;
        }
    }

    if (!captured) {
        void* stack[64] = {};
        const USHORT frameCount = CaptureStackBackTrace(0, 64, stack, nullptr);
        for (USHORT i = 0; i < frameCount; ++i)
            appendSymbolLine(lines, process, reinterpret_cast<DWORD64>(stack[i]));
    }

    if (lines.size() <= 1)
        lines.append(QStringLiteral("  (no stack frames captured)"));

    return lines.join(QLatin1Char('\n'));
}

void writeMinidumpWindows(EXCEPTION_POINTERS* info)
{
    const QString path = latestCrashDumpPath();
    HANDLE file = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_WRITE,
                              0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    MINIDUMP_EXCEPTION_INFORMATION mei = {};
    MINIDUMP_EXCEPTION_INFORMATION* meiPtr = nullptr;
    if (info) {
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = info;
        mei.ClientPointers = FALSE;
        meiPtr = &mei;
    }

    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpWithDataSegs,
                      meiPtr, nullptr, nullptr);
    CloseHandle(file);
}

QString captureHungMainThreadStack()
{
    if (g_mainThreadId == 0 || g_mainThreadId == GetCurrentThreadId())
        return QStringLiteral("Hung thread stack: (unavailable)");

    HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT
                                   | THREAD_QUERY_INFORMATION,
                               FALSE, g_mainThreadId);
    if (!thread)
        return QStringLiteral("Hung thread stack: (OpenThread failed)");

    SuspendThread(thread);
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_FULL;
    QString stack = QStringLiteral("Hung main-thread stack:");
    if (GetThreadContext(thread, &context)) {
        const QString walked = captureStackTraceWindows(&context);
        stack = walked.isEmpty() ? stack : walked;
        if (!stack.startsWith(QStringLiteral("Stack trace:")))
            stack.prepend(QStringLiteral("Hung main-thread stack:\n"));
        else
            stack.replace(0, QStringLiteral("Stack trace:").size(),
                          QStringLiteral("Hung main-thread stack:"));
    } else {
        stack += QStringLiteral("\n  (GetThreadContext failed)");
    }
    ResumeThread(thread);
    CloseHandle(thread);
    return stack;
}

void reportUiHang(int hungSeconds)
{
    if (g_shuttingDown || g_isCrashDialogProcess)
        return;

    const QString summary =
        QStringLiteral("UI hang / not responding (~%1s)").arg(hungSeconds);
    QStringList extra;
    extra.append(QStringLiteral(
        "Main thread did not process the event loop. Arachnel was frozen for the user."));
    extra.append(QStringLiteral("Main thread id: %1").arg(static_cast<qulonglong>(g_mainThreadId)));
    const QString stack = captureHungMainThreadStack();
    handleCrashReport(buildCrashReport(summary, extra.join(QStringLiteral("\n")), stack));
}

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* info)
{
    if (g_shuttingDown)
        return EXCEPTION_CONTINUE_SEARCH;

    if (!info || !info->ExceptionRecord || !info->ContextRecord)
        return EXCEPTION_CONTINUE_SEARCH;

    writeMinidumpWindows(info);

    const EXCEPTION_RECORD* record = info->ExceptionRecord;
    const DWORD code = record->ExceptionCode;
    QString summary = describeExceptionCode(code);
    summary += QStringLiteral(" at 0x%1")
                   .arg(reinterpret_cast<quintptr>(record->ExceptionAddress), QT_POINTER_SIZE * 2,
                        16, QLatin1Char('0'));

    QStringList extra;
    const QString avDetails = formatAccessViolationDetails(record);
    if (!avDetails.isEmpty())
        extra.append(avDetails);
    extra.append(QStringLiteral("Fault module: %1")
                     .arg(moduleForAddress(reinterpret_cast<DWORD64>(record->ExceptionAddress))));
    extra.append(QStringLiteral("Exception thread: %1").arg(GetCurrentThreadId()));

    CONTEXT context = *info->ContextRecord;
    const QString stackTrace = captureStackTraceWindows(&context);

    handleCrashReport(buildCrashReport(summary, extra.join(QStringLiteral("\n")), stackTrace));
    return EXCEPTION_CONTINUE_SEARCH;
}

void handleQtFatalMessage(const QString& message)
{
    const QString summary = QStringLiteral("Fatal Qt error");
    const QString extra = QStringLiteral("Message: %1").arg(message);
    const QString stackTrace = captureStackTraceWindows(nullptr);
    handleCrashReport(buildCrashReport(summary, extra, stackTrace));
}
#else
QString demangleSymbol(const char* symbol)
{
    if (!symbol)
        return QStringLiteral("?");

    QString text = QString::fromUtf8(symbol);
#if defined(__GNUC__)
    int status = 0;
    if (const char* nameStart = std::strchr(symbol, '(')) {
        if (const char* nameEnd = std::strchr(nameStart, '+')) {
            const QString mangled = QString::fromUtf8(nameStart + 1,
                                                      static_cast<int>(nameEnd - nameStart - 1));
            if (char* demangled = abi::__cxa_demangle(mangled.toUtf8().constData(), nullptr,
                                                      nullptr, &status)) {
                text = QString::fromUtf8(demangled);
                std::free(demangled);
            }
        }
    }
#endif
    return text;
}

QString moduleForAddressUnix(void* address)
{
    Dl_info info = {};
    if (dladdr(address, &info) == 0 || !info.dli_fname)
        return QStringLiteral("unknown module");
    return QDir::fromNativeSeparators(QString::fromUtf8(info.dli_fname));
}

QString captureStackTraceUnix()
{
    QStringList lines;
    lines.append(QStringLiteral("Stack trace:"));

    void* frames[64] = {};
    const int frameCount = backtrace(frames, 64);
    char** symbols = backtrace_symbols(frames, frameCount);
    for (int i = 0; i < frameCount; ++i) {
        const QString symbol = symbols && symbols[i] ? demangleSymbol(symbols[i])
                                                     : QStringLiteral("?");
        lines.append(QStringLiteral("  #%1 %2 [%3]")
                         .arg(i)
                         .arg(symbol)
                         .arg(moduleForAddressUnix(frames[i])));
    }
    if (symbols)
        free(symbols);

    if (lines.size() <= 1)
        lines.append(QStringLiteral("  (no stack frames captured)"));

    return lines.join(QLatin1Char('\n'));
}

QString describeUnixSignal(int signal, siginfo_t* info)
{
    QString summary;
    switch (signal) {
    case SIGSEGV:
        summary = QStringLiteral("Segmentation fault");
        break;
    case SIGABRT:
        summary = QStringLiteral("Abort");
        break;
    case SIGFPE:
        summary = QStringLiteral("Floating-point exception");
        break;
    case SIGILL:
        summary = QStringLiteral("Illegal instruction");
        break;
    case SIGBUS:
        summary = QStringLiteral("Bus error");
        break;
    default:
        summary = QStringLiteral("Signal %1").arg(signal);
        break;
    }

    if (info) {
        summary += QStringLiteral(" at 0x%1")
                       .arg(reinterpret_cast<quintptr>(info->si_addr), QT_POINTER_SIZE * 2, 16,
                            QLatin1Char('0'));
    }
    return summary;
}

void linuxSignalHandler(int signal, siginfo_t* info, void* context)
{
    Q_UNUSED(context);

    const QString summary = describeUnixSignal(signal, info);
    QStringList extra;
    if (info && signal == SIGSEGV) {
        extra.append(QStringLiteral("Fault address: 0x%1")
                         .arg(reinterpret_cast<quintptr>(info->si_addr), QT_POINTER_SIZE * 2, 16,
                              QLatin1Char('0')));
    }
    if (info) {
        extra.append(QStringLiteral("Fault module: %1")
                         .arg(moduleForAddressUnix(info->si_addr)));
    }

    handleCrashReport(
        buildCrashReport(summary, extra.join(QStringLiteral("\n")), captureStackTraceUnix()));
    _exit(128 + signal);
}

void handleQtFatalMessage(const QString& message)
{
    const QString summary = QStringLiteral("Fatal Qt error");
    const QString extra = QStringLiteral("Message: %1").arg(message);
    handleCrashReport(buildCrashReport(summary, extra, captureStackTraceUnix()));
}
#endif

#if !defined(Q_OS_WIN)
void reportUiHang(int hungSeconds)
{
    if (g_shuttingDown || g_isCrashDialogProcess)
        return;
    const QString summary =
        QStringLiteral("UI hang / not responding (~%1s)").arg(hungSeconds);
    const QString extra = QStringLiteral(
        "Main thread did not process the event loop. Arachnel was frozen for the user.");
    handleCrashReport(buildCrashReport(
        summary, extra, QStringLiteral("Hung thread stack: (not captured on this platform)")));
}
#endif

void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
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
            .arg(QDateTime::currentDateTime().toString(Qt::ISODate), QString::fromLatin1(level),
                 msg);

    QString fullLine = line;
    if (context.file && context.line > 0) {
        fullLine += QStringLiteral(" (%1:%2)").arg(QString::fromLocal8Bit(context.file),
                                                     context.line);
    }
    if (context.function && context.function[0] != '\0') {
        fullLine += QStringLiteral(" in %1()").arg(QString::fromLocal8Bit(context.function));
    }

    writeLine(fullLine, type != QtDebugMsg);

    if (type == QtFatalMsg) {
        handleQtFatalMessage(msg);
        abort();
    }
}


} // namespace arachnel

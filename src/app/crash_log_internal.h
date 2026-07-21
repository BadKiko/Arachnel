namespace {

QMutex g_logMutex;
QString g_logDir;
QString g_runExePath;
QString g_runArgsLine;
bool g_isCrashDialogProcess = false;
bool g_shuttingDown = false;
QStringList g_recentLines;

constexpr int kRecentLogLines = 80;

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

void rememberRecentLine(const QString& line)
{
    g_recentLines.append(line);
    while (g_recentLines.size() > kRecentLogLines)
        g_recentLines.removeFirst();
}

void writeLine(const QString& line, bool toStderr = true)
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

struct CrashReportData {
    QString summary;
    QString details;
    QString issueUrl;
};

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

    QStringList body;
    body.append(QStringLiteral("Arachnel %1").arg(QCoreApplication::applicationVersion()));
    body.append(QStringLiteral("Time: %1")
                    .arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
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

    if (!g_recentLines.isEmpty()) {
        body.append(QStringLiteral("Recent log (%1 lines):").arg(g_recentLines.size()));
        body.append(g_recentLines.join(QStringLiteral("\n")));
    }

    if (summary.contains(QStringLiteral("Access violation"))
        && extraDetails.contains(QStringLiteral("0x0000000000000001"))) {
        body.append(QStringLiteral(
            "Hint: null or invalid pointer read. If this appeared after updating Arachnel, "
            "rebuild and redeploy source plugins (run.ps1) so plugin DLLs match the app."));
    }

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

    const ULONG_PTR readAttempt = record->ExceptionInformation[0];
    const ULONG_PTR address = record->ExceptionInformation[1];
    const QString accessType = readAttempt ? QStringLiteral("read") : QStringLiteral("write");
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

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* info)
{
    if (g_shuttingDown)
        return EXCEPTION_CONTINUE_SEARCH;

    if (!info || !info->ExceptionRecord || !info->ContextRecord)
        return EXCEPTION_CONTINUE_SEARCH;

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

} // namespace

#include "windows_runner.h"

#include "proton_manager.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace arachnel::core {

namespace {

bool runProcess(QProcess& process, int timeoutMs, QString* errorOut)
{
    if (!process.waitForStarted(15000)) {
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось запустить: %1").arg(process.program());
        return false;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        if (errorOut)
            *errorOut = QStringLiteral("Таймаут: %1").arg(process.program());
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorOut) {
            const QString stderrText =
                QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *errorOut = stderrText.isEmpty()
                            ? QStringLiteral("%1 завершился с кодом %2")
                                  .arg(process.program())
                                  .arg(process.exitCode())
                            : stderrText;
        }
        return false;
    }
    return true;
}

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

QString formatWindowsParameters(const QStringList& arguments)
{
    QStringList parts;
    for (const QString& argument : arguments)
        parts << quoteWindowsArg(argument);
    return parts.join(QLatin1Char(' '));
}

QString describeWin32Error(DWORD error)
{
    if (error == ERROR_CANCELLED)
        return QStringLiteral("запуск отменён (UAC)");
    if (error == ERROR_ELEVATION_REQUIRED)
        return QStringLiteral("требуются права администратора");
    return QStringLiteral("Win32 %1").arg(error);
}

bool runWindowsNativeProcess(const QString& program, const QStringList& arguments, int timeoutMs,
                             QString* errorOut, const QString& workingDirectory)
{
    if (!QFileInfo::exists(program)) {
        if (errorOut)
            *errorOut = QStringLiteral("Файл не найден: %1").arg(program);
        return false;
    }

    const QString nativeProgram = QDir::toNativeSeparators(program);
    const QString parameters = formatWindowsParameters(arguments);
    const QString nativeWorkDir =
        workingDirectory.isEmpty() ? QString() : QDir::toNativeSeparators(workingDirectory);

    SHELLEXECUTEINFOW executeInfo{};
    executeInfo.cbSize = sizeof(executeInfo);
    executeInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOZONECHECKS;
    executeInfo.lpVerb = L"open";
    executeInfo.lpFile = reinterpret_cast<LPCWSTR>(nativeProgram.utf16());
    executeInfo.lpParameters = parameters.isEmpty()
                                   ? nullptr
                                   : reinterpret_cast<LPCWSTR>(parameters.utf16());
    executeInfo.lpDirectory = nativeWorkDir.isEmpty()
                                  ? nullptr
                                  : reinterpret_cast<LPCWSTR>(nativeWorkDir.utf16());
    executeInfo.nShow = SW_HIDE;

    if (!ShellExecuteExW(&executeInfo)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Не удалось запустить %1: %2")
                            .arg(nativeProgram, describeWin32Error(GetLastError()));
        }
        return false;
    }

    if (!executeInfo.hProcess) {
        if (errorOut)
            *errorOut = QStringLiteral("Не удалось отследить процесс установки");
        return false;
    }

    const DWORD waitResult =
        WaitForSingleObject(executeInfo.hProcess, static_cast<DWORD>(timeoutMs));
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(executeInfo.hProcess, 1);
        CloseHandle(executeInfo.hProcess);
        if (errorOut)
            *errorOut = QStringLiteral("Таймаут: %1").arg(nativeProgram);
        return false;
    }

    DWORD exitCode = 1;
    GetExitCodeProcess(executeInfo.hProcess, &exitCode);
    CloseHandle(executeInfo.hProcess);

    if (exitCode != 0) {
        if (errorOut)
            *errorOut = QStringLiteral("%1 завершился с кодом %2").arg(nativeProgram).arg(exitCode);
        return false;
    }

    return true;
}

#endif

bool isWindowsExecutable(const QString& path)
{
    return path.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive);
}

} // namespace

void fillProtonInstallFields(const QString& entryId, const QString& preferredProtonId,
                             QString* protonExecutable, QString* compatDataPath,
                             QString* steamCompatClientPath)
{
#if !defined(Q_OS_LINUX)
    (void)entryId;
    (void)preferredProtonId;
    (void)protonExecutable;
    (void)compatDataPath;
    (void)steamCompatClientPath;
    return;
#else
    if (!protonExecutable || !compatDataPath || !steamCompatClientPath)
        return;

    ProtonManager manager;
    const QString proton = manager.executableForId(preferredProtonId);
    if (proton.isEmpty()) {
        const QString fallback = manager.resolveProtonExecutable(preferredProtonId);
        if (fallback.isEmpty())
            return;
        *protonExecutable = fallback;
    } else {
        *protonExecutable = proton;
    }
    *compatDataPath = manager.compatDataPathForGame(entryId);
    *steamCompatClientPath = manager.steamCompatClientPath();
#endif
}

bool runWindowsProgramAndWait(const QString& program, const QStringList& arguments, int timeoutMs,
                              QString* errorOut, const QString& workingDirectory,
                              const WindowsRunEnv& env)
{
#if defined(Q_OS_WIN)
    (void)env;
    return runWindowsNativeProcess(program, arguments, timeoutMs, errorOut, workingDirectory);
#else
    if (!QFileInfo::exists(program)) {
        if (errorOut)
            *errorOut = QStringLiteral("Файл не найден: %1").arg(program);
        return false;
    }

    QString workDir = workingDirectory;
    if (workDir.isEmpty())
        workDir = QFileInfo(program).absolutePath();

    if (!isWindowsExecutable(program)) {
        QProcess process;
        process.setProgram(program);
        process.setArguments(arguments);
        process.setWorkingDirectory(workDir);
        return runProcess(process, timeoutMs, errorOut);
    }

    if (!env.useProton()) {
        if (errorOut) {
            *errorOut = QStringLiteral(
                "Для установки Windows-установщика нужен Proton (Настройки → Запуск)");
        }
        return false;
    }

    QProcessEnvironment qenv = QProcessEnvironment::systemEnvironment();
    if (!env.steamCompatClientPath.isEmpty())
        qenv.insert(QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH"), env.steamCompatClientPath);
    if (!env.compatDataPath.isEmpty())
        qenv.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"), env.compatDataPath);
    qenv.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));

    QStringList protonArgs = {QStringLiteral("run"), program};
    protonArgs += arguments;

    QProcess process;
    process.setProgram(env.protonExecutable);
    process.setArguments(protonArgs);
    process.setWorkingDirectory(workDir);
    process.setProcessEnvironment(qenv);
    return runProcess(process, timeoutMs, errorOut);
#endif
}

} // namespace arachnel::core

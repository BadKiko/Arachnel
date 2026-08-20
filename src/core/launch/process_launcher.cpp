#include "process_launcher.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>

#if defined(Q_OS_UNIX)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace arachnel::core {

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

    QProcess process;
    process.setProgram(launch.program);
    process.setArguments(launch.arguments);
    process.setWorkingDirectory(workDir);
    process.setProcessEnvironment(launch.environment);

    const QString capturePath = logFilePath.trimmed();
#if defined(Q_OS_UNIX)
    // Pre-encode in the parent: the child modifier runs after fork() where
    // allocating a QString/utf8 is not async-signal-safe.
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
#else
    if (!capturePath.isEmpty()) {
        process.setStandardOutputFile(capturePath);
        process.setStandardErrorFile(capturePath);
    }
#endif

    qint64 processId = 0;
    const bool ok = process.startDetached(&processId);
    if (!ok && errorOut)
        *errorOut = QCoreApplication::translate("Core", "Failed to start process");
    if (ok && processIdOut)
        *processIdOut = processId;
    return ok;
}

} // namespace arachnel::core

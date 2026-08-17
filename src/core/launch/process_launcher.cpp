#include "process_launcher.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>

#if defined(Q_OS_UNIX)
#include <unistd.h>
#endif

namespace arachnel::core {

bool ProcessLauncher::launch(const ResolvedLaunch& launch, QString* errorOut, qint64* processIdOut)
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
#if defined(Q_OS_UNIX)
    process.setChildProcessModifier([]() { ::setpgid(0, 0); });
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

#include "process_launcher.h"

#include <QFileInfo>
#include <QProcess>

namespace arachnel::core {

bool ProcessLauncher::launch(const ResolvedLaunch& launch, QString* errorOut, qint64* processIdOut)
{
    if (launch.program.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Исполняемый файл не задан");
        return false;
    }

    QFileInfo programInfo(launch.program);
    if (!programInfo.exists()) {
        if (errorOut)
            *errorOut = QStringLiteral("Файл не найден: %1").arg(launch.program);
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

    qint64 processId = 0;
    const bool ok = process.startDetached(&processId);
    if (!ok && errorOut)
        *errorOut = QStringLiteral("Не удалось запустить процесс");
    if (ok && processIdOut)
        *processIdOut = processId;
    return ok;
}

} // namespace arachnel::core

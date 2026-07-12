#include "process_launcher.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

namespace arachnel::core {

bool ProcessLauncher::launch(const LaunchInfo& info, QString* errorOut, qint64* processIdOut)
{
    if (info.executable.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Исполняемый файл не задан");
        return false;
    }

    QFileInfo exeInfo(info.executable);
    if (!exeInfo.exists()) {
        if (errorOut)
            *errorOut = QStringLiteral("Файл не найден: %1").arg(info.executable);
        return false;
    }

    QString workDir = info.workingDirectory;
    if (workDir.isEmpty())
        workDir = exeInfo.absolutePath();

    qint64 processId = 0;
    const bool ok =
        QProcess::startDetached(info.executable, info.arguments, workDir, &processId);
    if (!ok && errorOut)
        *errorOut = QStringLiteral("Не удалось запустить процесс");
    if (ok && processIdOut)
        *processIdOut = processId;
    return ok;
}

} // namespace arachnel::core

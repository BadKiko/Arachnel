#include "process_launcher.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

namespace arachnel::core {

bool ProcessLauncher::launch(const LaunchInfo& info, QString* errorOut)
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

    const bool ok = QProcess::startDetached(info.executable, info.arguments, workDir);
    if (!ok && errorOut)
        *errorOut = QStringLiteral("Не удалось запустить процесс");
    return ok;
}

} // namespace arachnel::core

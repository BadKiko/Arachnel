#include "desktop_shortcut_service.h"

#include "game_launch_target.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <sys/stat.h>
#endif

namespace arachnel::core {

namespace {

#if defined(Q_OS_WIN)
QString psSingleQuoted(const QString& value)
{
    QString out = value;
    out.replace(QLatin1Char('\''), QStringLiteral("''"));
    return out;
}

bool createWindowsLnk(const QString& linkPath, const GameLaunchTarget& target, QString* errorOut)
{
    const QString args = joinLaunchArguments(target.arguments);
    const QString workDir = target.workingDirectory.isEmpty()
                                ? QFileInfo(target.executable).absolutePath()
                                : target.workingDirectory;

    const QString script =
        QStringLiteral("$s = (New-Object -ComObject WScript.Shell).CreateShortcut('%1'); "
                       "$s.TargetPath = '%2'; "
                       "$s.Arguments = '%3'; "
                       "$s.WorkingDirectory = '%4'; "
                       "$s.IconLocation = '%2,0'; "
                       "$s.Description = '%5'; "
                       "$s.Save()")
            .arg(psSingleQuoted(QDir::toNativeSeparators(linkPath)),
                 psSingleQuoted(QDir::toNativeSeparators(target.executable)),
                 psSingleQuoted(args),
                 psSingleQuoted(QDir::toNativeSeparators(workDir)),
                 psSingleQuoted(target.title));

    QProcess process;
    process.setProgram(QStringLiteral("powershell"));
    process.setArguments({QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"),
                          QStringLiteral("Bypass"), QStringLiteral("-Command"), script});
    process.start();
    if (!process.waitForFinished(30000) || process.exitCode() != 0) {
        if (errorOut) {
            const QString err = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *errorOut = err.isEmpty() ? QStringLiteral("Failed to create shortcut") : err;
        }
        return false;
    }
    return QFileInfo::exists(linkPath);
}
#endif

#if defined(Q_OS_LINUX)
QString escapeDesktopValue(const QString& value)
{
    QString out = value;
    out.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    out.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    return out;
}

QString quoteDesktopExecArg(const QString& arg)
{
    // desktop-file Exec: quote if needed; escape " and \.
    if (arg.isEmpty())
        return QStringLiteral("\"\"");
    const bool needQuote = arg.contains(QLatin1Char(' ')) || arg.contains(QLatin1Char('\t'))
        || arg.contains(QLatin1Char('"')) || arg.contains(QLatin1Char('\\'))
        || arg.contains(QLatin1Char('$')) || arg.contains(QLatin1Char('`'));
    if (!needQuote)
        return arg;
    QString escaped = arg;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

bool createLinuxDesktop(const QString& linkPath, const GameLaunchTarget& target, QString* errorOut)
{
    QStringList execParts;
    execParts.append(quoteDesktopExecArg(target.executable));
    for (const QString& arg : target.arguments)
        execParts.append(quoteDesktopExecArg(arg));

    const QString workDir = target.workingDirectory.isEmpty()
                                ? QFileInfo(target.executable).absolutePath()
                                : target.workingDirectory;

    QFile file(linkPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not write desktop file");
        return false;
    }

    QTextStream out(&file);
    out << QStringLiteral("[Desktop Entry]\n");
    out << QStringLiteral("Type=Application\n");
    out << QStringLiteral("Version=1.0\n");
    out << QStringLiteral("Name=") << escapeDesktopValue(target.title) << QLatin1Char('\n');
    out << QStringLiteral("Exec=") << execParts.join(QLatin1Char(' ')) << QLatin1Char('\n');
    out << QStringLiteral("Path=") << escapeDesktopValue(workDir) << QLatin1Char('\n');
    out << QStringLiteral("Icon=") << escapeDesktopValue(target.executable) << QLatin1Char('\n');
    out << QStringLiteral("Terminal=false\n");
    out << QStringLiteral("Categories=Game;\n");
    out << QStringLiteral("StartupNotify=true\n");
    file.close();

#if defined(Q_OS_UNIX)
    ::chmod(QFile::encodeName(linkPath).constData(), 0755);
#endif
    return QFileInfo::exists(linkPath);
}
#endif

} // namespace

bool createOsShortcut(const QString& linkPath, const GameLaunchTarget& target, QString* errorOut)
{
    if (target.executable.isEmpty() || !QFileInfo::exists(target.executable)) {
        if (errorOut)
            *errorOut = QStringLiteral("Executable not found");
        return false;
    }

    const QFileInfo linkInfo(linkPath);
    if (!QDir().mkpath(linkInfo.absolutePath())) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not create shortcut folder");
        return false;
    }

#if defined(Q_OS_WIN)
    return createWindowsLnk(linkPath, target, errorOut);
#elif defined(Q_OS_LINUX)
    return createLinuxDesktop(linkPath, target, errorOut);
#else
    if (errorOut)
        *errorOut = QStringLiteral("Shortcuts are not supported on this platform");
    return false;
#endif
}

} // namespace arachnel::core

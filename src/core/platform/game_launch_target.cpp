#include "game_launch_target.h"

#include "install_heuristics.h"
#include "launch_resolver.h"

#include <QFileInfo>

namespace arachnel::core {

QString joinLaunchArguments(const QStringList& args)
{
    QStringList quoted;
    quoted.reserve(args.size());
    for (const QString& arg : args) {
        if (arg.contains(QLatin1Char(' ')) || arg.contains(QLatin1Char('\t'))
            || arg.contains(QLatin1Char('"'))) {
            QString escaped = arg;
            escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
            quoted.append(QLatin1Char('"') + escaped + QLatin1Char('"'));
        } else {
            quoted.append(arg);
        }
    }
    return quoted.join(QLatin1Char(' '));
}

QString sanitizeShortcutFileName(const QString& title)
{
    QString name = title.trimmed();
    if (name.isEmpty())
        name = QStringLiteral("Game");
    const QString banned = QStringLiteral("<>:\"/\\|?*");
    for (const QChar ch : banned)
        name.replace(ch, QLatin1Char('_'));
    while (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' ')))
        name.chop(1);
    if (name.isEmpty())
        name = QStringLiteral("Game");
    return name;
}

GameLaunchTarget resolveGameLaunchTarget(const LibraryGame& game, const LaunchInfo& pluginInfo,
                                         const SettingsStore& settings)
{
    GameLaunchTarget target;
    target.title = game.title.isEmpty() ? game.id : game.title;

    LaunchInfo info = pluginInfo;
    QString overrideExe = game.executableOverride.trimmed();
    if (!overrideExe.isEmpty()
        && isExcludedGameExecutable(QFileInfo(overrideExe).fileName())) {
        overrideExe.clear();
    }

    if (info.executable.isEmpty() && overrideExe.isEmpty()) {
        const QString found = findGameExecutableInTree(game.installPath);
        if (!found.isEmpty())
            info.executable = found;
    } else if (!info.executable.isEmpty()
               && isExcludedGameExecutable(QFileInfo(info.executable).fileName())) {
        info.executable.clear();
        const QString found = findGameExecutableInTree(game.installPath);
        if (!found.isEmpty())
            info.executable = found;
    }

    QString executable = overrideExe;
    if (executable.isEmpty())
        executable = info.executable;
    if (executable.isEmpty())
        return target;

    target.executable = QFileInfo(executable).absoluteFilePath();

    QString workDir = info.workingDirectory;
    if (workDir.isEmpty())
        workDir = QFileInfo(target.executable).absolutePath();
    if (workDir.isEmpty() && !game.installPath.isEmpty())
        workDir = game.installPath;
    target.workingDirectory = workDir;

    // Shortcut / Steam: game exe + args only (no Proton wrapper).
    target.arguments = info.argumentsPrefix;
    target.arguments += info.arguments;
    target.arguments += splitLaunchArguments(settings.globalLaunchArgs());
    target.arguments += splitLaunchArguments(game.launchArgs);
    return target;
}

} // namespace arachnel::core

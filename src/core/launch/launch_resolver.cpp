#include "launch_resolver.h"

#include "proton_manager.h"

#include <QDir>
#include <QFileInfo>

namespace arachnel::core {

QStringList splitLaunchArguments(const QString& text)
{
    QStringList result;
    QString current;
    bool inQuotes = false;

    const QString trimmed = text.trimmed();
    for (int i = 0; i < trimmed.size(); ++i) {
        const QChar ch = trimmed.at(i);
        if (ch == QLatin1Char('"')) {
            inQuotes = !inQuotes;
            continue;
        }
        if ((ch == QLatin1Char(' ') || ch == QLatin1Char('\t')) && !inQuotes) {
            if (!current.isEmpty()) {
                result.append(current);
                current.clear();
            }
            continue;
        }
        current.append(ch);
    }

    if (!current.isEmpty())
        result.append(current);
    return result;
}

namespace {

bool shouldUseProton(const QString& executable)
{
#if !defined(Q_OS_LINUX)
    (void)executable;
    return false;
#else
    return executable.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive);
#endif
}

QProcessEnvironment buildProtonEnvironment(const QString& gameId, const QString& protonInstallDir,
                                           ProtonManager& manager)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH"), manager.steamCompatClientPath());
    env.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"), manager.compatDataPathForGame(gameId));
    env.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));
    if (!protonInstallDir.trimmed().isEmpty())
        env.insert(QStringLiteral("PROTON_PATH"), protonInstallDir);
    return env;
}

} // namespace

ResolvedLaunch resolveLaunch(const LaunchInfo& pluginInfo, const LibraryGame& game,
                             const SettingsStore& settings)
{
    ResolvedLaunch resolved;
    if (pluginInfo.executable.isEmpty() && game.executableOverride.trimmed().isEmpty())
        return resolved;

    QString executable = game.executableOverride.trimmed();
    if (executable.isEmpty())
        executable = pluginInfo.executable;

    QString workDir = pluginInfo.workingDirectory;
    if (workDir.isEmpty())
        workDir = QFileInfo(executable).absolutePath();

    QStringList arguments = pluginInfo.arguments;
    arguments += splitLaunchArguments(settings.globalLaunchArgs());
    arguments += splitLaunchArguments(game.launchArgs);

    const bool useProton = shouldUseProton(executable);

    if (useProton) {
        ProtonManager manager;
        const QString protonId = settings.resolvedProtonId(game.protonId, manager);
        const QString proton = manager.executableForId(protonId);
        if (proton.isEmpty())
            return resolved;

        resolved.program = proton;
        resolved.arguments = pluginInfo.argumentsPrefix;
        resolved.arguments += QStringList{QStringLiteral("run"), executable};
        resolved.arguments += arguments;
        resolved.workingDirectory = workDir;
        resolved.environment =
            buildProtonEnvironment(game.id, manager.installDirForId(protonId), manager);

        if (!pluginInfo.wineDllOverrides.trimmed().isEmpty()) {
            const QString existing = resolved.environment.value(QStringLiteral("WINEDLLOVERRIDES"));
            const QString merged = existing.isEmpty()
                                       ? pluginInfo.wineDllOverrides
                                       : existing + QLatin1Char(';') + pluginInfo.wineDllOverrides;
            resolved.environment.insert(QStringLiteral("WINEDLLOVERRIDES"), merged);
        }

        for (auto it = pluginInfo.environmentExtras.constBegin();
             it != pluginInfo.environmentExtras.constEnd(); ++it) {
            if (it.key().isEmpty())
                continue;
            if (it.key() == QStringLiteral("LD_PRELOAD")) {
                const QString existing = resolved.environment.value(QStringLiteral("LD_PRELOAD"));
                QString added = it.value().trimmed();
                while (added.startsWith(QLatin1Char(':')))
                    added.remove(0, 1);
                resolved.environment.insert(QStringLiteral("LD_PRELOAD"),
                                            existing.isEmpty() ? added
                                                               : existing + QLatin1Char(':') + added);
            } else {
                resolved.environment.insert(it.key(), it.value());
            }
        }
        return resolved;
    }

    resolved.program = executable;
    resolved.arguments = pluginInfo.argumentsPrefix + arguments;
    resolved.workingDirectory = workDir;
    resolved.environment = QProcessEnvironment::systemEnvironment();
    for (auto it = pluginInfo.environmentExtras.constBegin();
         it != pluginInfo.environmentExtras.constEnd(); ++it) {
        if (it.key().isEmpty())
            continue;
        if (it.key() == QStringLiteral("LD_PRELOAD")) {
            const QString existing = resolved.environment.value(QStringLiteral("LD_PRELOAD"));
            QString added = it.value().trimmed();
            while (added.startsWith(QLatin1Char(':')))
                added.remove(0, 1);
            resolved.environment.insert(QStringLiteral("LD_PRELOAD"),
                                        existing.isEmpty() ? added
                                                           : existing + QLatin1Char(':') + added);
        } else {
            resolved.environment.insert(it.key(), it.value());
        }
    }
    if (!pluginInfo.wineDllOverrides.trimmed().isEmpty()) {
        const QString existing = resolved.environment.value(QStringLiteral("WINEDLLOVERRIDES"));
        const QString merged = existing.isEmpty() ? pluginInfo.wineDllOverrides
                                                  : existing + QLatin1Char(';') + pluginInfo.wineDllOverrides;
        resolved.environment.insert(QStringLiteral("WINEDLLOVERRIDES"), merged);
    }
    return resolved;
}

} // namespace arachnel::core

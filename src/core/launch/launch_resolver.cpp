#include "launch_resolver.h"

#include "install_heuristics.h"
#include "proton_manager.h"

#include <QDir>
#include <QFile>
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
    // Host Steam / NixOS sessions often inject steam-runtime into LD_LIBRARY_PATH.
    // That breaks /usr/bin/env (ATTR_1.3) and unrelated host tools. Keep a clean
    // baseline; Proton and optional run.sh set up what they need themselves.
    env.remove(QStringLiteral("LD_LIBRARY_PATH"));
    env.remove(QStringLiteral("STEAM_RUNTIME"));
    env.remove(QStringLiteral("STEAM_RUNTIME_LIBRARY_PATH"));
    env.insert(QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH"), manager.steamCompatClientPath());
    env.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"), manager.compatDataPathForGame(gameId));
    env.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));
    if (!protonInstallDir.trimmed().isEmpty())
        env.insert(QStringLiteral("PROTON_PATH"), protonInstallDir);
    return env;
}

bool hostBreaksWithLegacySteamRuntime()
{
    // Legacy ubuntu12_32/steam-runtime/run.sh puts old libs on LD_LIBRARY_PATH.
    // NixOS / ostree / Bazzite then fail shebang `/usr/bin/env` with libattr ATTR_1.3.
    if (QFileInfo::exists(QStringLiteral("/etc/NIXOS")))
        return true;
    if (QFileInfo::exists(QStringLiteral("/run/ostree-booted")))
        return true;
    if (!qEnvironmentVariableIsEmpty("NIX_STORE") || !qEnvironmentVariableIsEmpty("NIX_PATH"))
        return true;
    QFile osRelease(QStringLiteral("/etc/os-release"));
    if (osRelease.open(QIODevice::ReadOnly)) {
        const QByteArray text = osRelease.readAll().toLower();
        if (text.contains("bazzite") || text.contains("silverblue")
            || text.contains("kinoite"))
            return true;
    }
    const QByteArray ld = qgetenv("LD_LIBRARY_PATH");
    if (ld.contains("steam-runtime") && ld.contains("libattr"))
        return true;
    return false;
}

QString filterOverlayPreloadForHost(const QString& preload)
{
    // 32-bit gameoverlayrenderer cannot be preloaded into 64-bit Proton; keep 64-bit only.
    QStringList kept;
    for (const QString& part : preload.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
        const QString p = part.trimmed();
        if (p.isEmpty())
            continue;
        if (p.contains(QStringLiteral("ubuntu12_32/")))
            continue;
        kept.append(p);
    }
    return kept.join(QLatin1Char(':'));
}

} // namespace

ResolvedLaunch resolveLaunch(const LaunchInfo& pluginInfo, const LibraryGame& game,
                             const SettingsStore& settings, ProtonManager* protonManager)
{
    ResolvedLaunch resolved;

    QString overrideExe = game.executableOverride.trimmed();
    if (!overrideExe.isEmpty()
        && isExcludedGameExecutable(QFileInfo(overrideExe).fileName())) {
        overrideExe.clear();
    }

    QString pluginExe = pluginInfo.executable;
    if (!pluginExe.isEmpty()
        && isExcludedGameExecutable(QFileInfo(pluginExe).fileName())) {
        pluginExe.clear();
    }

    if (pluginExe.isEmpty() && overrideExe.isEmpty())
        return resolved;

    QString executable = overrideExe;
    if (executable.isEmpty())
        executable = pluginExe;

    QString workDir = pluginInfo.workingDirectory;
    if (workDir.isEmpty())
        workDir = QFileInfo(executable).absolutePath();

    QStringList arguments = pluginInfo.arguments;
    arguments += splitLaunchArguments(settings.globalLaunchArgs());
    arguments += splitLaunchArguments(game.launchArgs);

    const bool useProton = shouldUseProton(executable);

    if (useProton) {
        ProtonManager localManager;
        ProtonManager& manager = protonManager ? *protonManager : localManager;
        const QString protonId = settings.resolvedProtonId(game.protonId, manager);
        const QString proton = manager.executableForId(protonId);
        if (proton.isEmpty())
            return resolved;

        QStringList protonArgs = pluginInfo.argumentsPrefix;
        protonArgs += QStringList{QStringLiteral("run"), executable};
        protonArgs += arguments;

        // SOFL Online-Fix: optionally prefix with legacy steam-runtime/run.sh.
        // Skip legacy on NixOS / hosts where steam-runtime breaks /usr/bin/env.
        const QString runtimeMode =
            pluginInfo.environmentExtras.value(QStringLiteral("ARACHNEL_USE_STEAM_RUNTIME"));
        const bool allowLegacy = runtimeMode == QStringLiteral("legacy")
                                 && !hostBreaksWithLegacySteamRuntime();
        if (allowLegacy) {
            const QString legacyRuntime = manager.findLegacySteamRuntime();
            if (!legacyRuntime.isEmpty()) {
                resolved.program = legacyRuntime;
                resolved.arguments = QStringList{proton} + protonArgs;
            } else {
                resolved.program = proton;
                resolved.arguments = protonArgs;
            }
        } else if (runtimeMode == QStringLiteral("1") && !hostBreaksWithLegacySteamRuntime()) {
            const QString steamRuntime = manager.findSteamLinuxRuntime();
            if (!steamRuntime.isEmpty() && manager.steamLinuxRuntimeUsable()) {
                resolved.program = steamRuntime;
                resolved.arguments = QStringList{proton} + protonArgs;
            } else if (!steamRuntime.isEmpty() && manager.canAaExecSteamProfile()) {
                resolved.program = QStringLiteral("/usr/bin/aa-exec");
                resolved.arguments =
                    QStringList{QStringLiteral("-p"), QStringLiteral("steam"), QStringLiteral("--"),
                                steamRuntime, proton}
                    + protonArgs;
            } else {
                resolved.program = proton;
                resolved.arguments = protonArgs;
            }
        } else {
            resolved.program = proton;
            resolved.arguments = protonArgs;
        }
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

        // Skip internal launch hints when applying env extras.
        for (auto it = pluginInfo.environmentExtras.constBegin();
             it != pluginInfo.environmentExtras.constEnd(); ++it) {
            if (it.key().isEmpty() || it.key() == QStringLiteral("ARACHNEL_USE_STEAM_RUNTIME"))
                continue;
            if (it.key() == QStringLiteral("LD_PRELOAD")) {
                const QString existing = resolved.environment.value(QStringLiteral("LD_PRELOAD"));
                QString added = filterOverlayPreloadForHost(it.value().trimmed());
                while (added.startsWith(QLatin1Char(':')))
                    added.remove(0, 1);
                if (added.isEmpty())
                    continue;
                resolved.environment.insert(QStringLiteral("LD_PRELOAD"),
                                            existing.isEmpty() ? added
                                                               : existing + QLatin1Char(':') + added);
            } else if (it.key() == QStringLiteral("LD_LIBRARY_PATH")) {
                // Never inherit Steam-runtime library paths from extras.
                continue;
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
        if (it.key().isEmpty() || it.key() == QStringLiteral("ARACHNEL_USE_STEAM_RUNTIME"))
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

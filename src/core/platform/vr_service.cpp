#include "vr_service.h"

#include "online_fix_overlay.h"
#include "steam_shortcut_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace arachnel::core {

namespace {

bool shouldSkipScanDir(const QString& name)
{
    const QString lower = name.toLower();
    return lower == QLatin1String(".depotdownloader") || lower == QLatin1String("staging")
        || lower == QLatin1String("intermediate") || lower == QLatin1String("saved")
        || lower == QLatin1String("thirdparty") || lower == QLatin1String(".git");
}

bool hasExplicitVrArgument(const QStringList& args)
{
    for (const QString& arg : args) {
        const QString lower = arg.trimmed().toLower();
        if (lower == QStringLiteral("-vr") || lower == QStringLiteral("/vr")
            || lower.startsWith(QStringLiteral("-vrmode")) || lower.startsWith(QStringLiteral("/vrmode"))
            || lower.startsWith(QStringLiteral("-openxr")) || lower.startsWith(QStringLiteral("-useopenxr"))
            || lower.startsWith(QStringLiteral("-hmd="))) {
            return true;
        }
    }
    return false;
}

bool isVrExecutableStem(const QString& stem)
{
    const QString lower = stem.toLower();
    return lower.endsWith(QStringLiteral("_vr")) || lower.endsWith(QStringLiteral("-vr"))
        || lower == QStringLiteral("vr") || lower == QStringLiteral("hlvr")
        || lower.contains(QStringLiteral("_vr_")) || lower.endsWith(QStringLiteral("vr"));
}

} // namespace

QString vrRuntimeName(VrRuntimeKind kind)
{
    switch (kind) {
    case VrRuntimeKind::OpenVR:
        return QStringLiteral("OpenVR (SteamVR)");
    case VrRuntimeKind::OpenXR:
        return QStringLiteral("OpenXR");
    case VrRuntimeKind::Oculus:
        return QStringLiteral("Oculus VR");
    case VrRuntimeKind::None:
    default:
        return QStringLiteral("None");
    }
}

bool isSteamVrRunning()
{
#if defined(Q_OS_WIN)
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return false;
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Process32FirstW(snap, &entry)) {
        do {
            const QString name = QString::fromWCharArray(entry.szExeFile).toLower();
            if (name == QStringLiteral("vrserver.exe") || name == QStringLiteral("vrmonitor.exe")
                || name == QStringLiteral("vrcompositor.exe") || name == QStringLiteral("vrstartup.exe")) {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return found;
#else
    QProcess process;
    process.start(QStringLiteral("pgrep"), {QStringLiteral("-x"), QStringLiteral("vrserver")});
    if (process.waitForFinished(3000) && process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0)
        return true;
    process.start(QStringLiteral("pgrep"), {QStringLiteral("-x"), QStringLiteral("vrmonitor")});
    return process.waitForFinished(3000) && process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0;
#endif
}

QString findSteamVrInstallPath()
{
    QStringList configPaths;
#if defined(Q_OS_WIN)
    const QString localAppData = QString::fromLocal8Bit(qgetenv("LOCALAPPDATA"));
    if (!localAppData.isEmpty())
        configPaths.append(localAppData + QStringLiteral("/openvr/openvrpaths.vrpath"));
    configPaths.append(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                       + QStringLiteral("/openvr/openvrpaths.vrpath"));
#else
    configPaths.append(QDir::homePath() + QStringLiteral("/.config/openvr/openvrpaths.vrpath"));
#endif

    for (const QString& configPath : configPaths) {
        if (!QFileInfo::exists(configPath))
            continue;
        QFile file(configPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (!doc.isObject())
            continue;
        const QJsonArray runtimes = doc.object().value(QStringLiteral("runtime")).toArray();
        for (const QJsonValue& val : runtimes) {
            const QString candidate = QDir::fromNativeSeparators(val.toString());
            if (candidate.isEmpty() || !QDir(candidate).exists())
                continue;
#if defined(Q_OS_WIN)
            if (QFileInfo::exists(candidate + QStringLiteral("/bin/win64/vrstartup.exe"))
                || QFileInfo::exists(candidate + QStringLiteral("/bin/win64/vrmonitor.exe"))
                || QFileInfo::exists(candidate + QStringLiteral("/bin/win32/vrstartup.exe"))) {
                return candidate;
            }
#else
            if (QFileInfo::exists(candidate + QStringLiteral("/bin/linux64/vrstartup.sh"))
                || QFileInfo::exists(candidate + QStringLiteral("/bin/vrstartup.sh"))) {
                return candidate;
            }
#endif
        }
    }

    const QString steamPath = findSteamInstallPath();
    if (!steamPath.isEmpty()) {
        const QString steamVrCandidate = steamPath + QStringLiteral("/steamapps/common/SteamVR");
        if (QDir(steamVrCandidate).exists())
            return steamVrCandidate;
    }

#if defined(Q_OS_WIN)
    const QStringList regKeys = {
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 250820"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 250820"),
    };
    for (const QString& key : regKeys) {
        QSettings settings(key, QSettings::NativeFormat);
        const QString loc = settings.value(QStringLiteral("InstallLocation")).toString();
        if (!loc.isEmpty() && QDir(loc).exists())
            return QDir::fromNativeSeparators(loc);
    }
#endif

    return {};
}

bool tryStartSteamVr()
{
    if (isSteamVrRunning())
        return true;

    const QString vrPath = findSteamVrInstallPath();
    if (!vrPath.isEmpty()) {
#if defined(Q_OS_WIN)
        QString startup = vrPath + QStringLiteral("/bin/win64/vrstartup.exe");
        if (!QFileInfo::exists(startup))
            startup = vrPath + QStringLiteral("/bin/win64/vrmonitor.exe");
        if (!QFileInfo::exists(startup))
            startup = vrPath + QStringLiteral("/bin/win32/vrstartup.exe");
#else
        QString startup = vrPath + QStringLiteral("/bin/linux64/vrstartup.sh");
        if (!QFileInfo::exists(startup))
            startup = vrPath + QStringLiteral("/bin/vrstartup.sh");
#endif
        if (QFileInfo::exists(startup)) {
            qint64 pid = 0;
            if (QProcess::startDetached(startup, {}, QFileInfo(startup).absolutePath(), &pid))
                return true;
        }
    }

    const QString steamPath = findSteamInstallPath();
    if (!steamPath.isEmpty()) {
#if defined(Q_OS_WIN)
        const QString steamExe = steamPath + QStringLiteral("/steam.exe");
        if (QFileInfo::exists(steamExe)) {
            qint64 pid = 0;
            if (QProcess::startDetached(steamExe, {QStringLiteral("-applaunch"), QStringLiteral("250820")},
                                        steamPath, &pid)) {
                return true;
            }
        }
#else
        QProcess process;
        qint64 pid = 0;
        if (QProcess::startDetached(QStringLiteral("steam"), {QStringLiteral("steam://run/250820")}, {}, &pid))
            return true;
#endif
    }

    return false;
}

VrGameDetection detectVrGame(const QString& installPath, const QString& executablePath,
                             const QString& genres, const QString& gameTitle,
                             const QVector<GameLaunchOption>& options,
                             const QString& selectedOptionId,
                             const QStringList& currentArgs)
{
    VrGameDetection detection;

    const bool explicitVrInArgs = hasExplicitVrArgument(currentArgs);

    const QString genresLower = genres.toLower();
    const bool genreMentionsVr = genresLower.contains(QStringLiteral("vr"))
        || genresLower.contains(QStringLiteral("virtual reality"))
        || genresLower.contains(QStringLiteral("виртуальная реальность"));
    const bool genreVrOnly = genresLower.contains(QStringLiteral("vr only"))
        || genresLower.contains(QStringLiteral("vr-only"))
        || genresLower.contains(QStringLiteral("только vr"));
    const bool genreVrSupport = genresLower.contains(QStringLiteral("vr support"))
        || genresLower.contains(QStringLiteral("vr supported"))
        || genresLower.contains(QStringLiteral("поддержка vr"));

    const QString titleLower = gameTitle.toLower();
    const bool titleIsExplicitVr = titleLower.endsWith(QStringLiteral(" vr"))
        || titleLower.contains(QStringLiteral(" vr:")) || titleLower.contains(QStringLiteral(" vr -"))
        || titleLower.endsWith(QStringLiteral(" (vr)"));

    bool hasVrOption = false;
    bool hasNonVrOption = false;
    for (const auto& opt : options) {
        const bool optIsVr = opt.type.compare(QStringLiteral("vr"), Qt::CaseInsensitive) == 0
            || opt.title.contains(QStringLiteral("VR"), Qt::CaseInsensitive)
            || hasExplicitVrArgument(opt.arguments)
            || isVrExecutableStem(QFileInfo(opt.executable).completeBaseName());
        if (optIsVr)
            hasVrOption = true;
        else
            hasNonVrOption = true;
    }

    QStringList foundVrExecutables;
    QStringList foundNonVrExecutables;

    if (!installPath.isEmpty() && QFileInfo::exists(installPath)) {
        const QDir root(installPath);

        // Scan root for engine markers
        if (root.exists(QStringLiteral("Engine"))
            || !root.entryList({QStringLiteral("*_Data")}, QDir::Dirs).isEmpty()) {
            if (!root.entryList({QStringLiteral("*_Data")}, QDir::Dirs).isEmpty()
                || root.exists(QStringLiteral("UnityPlayer.dll"))
                || root.exists(QStringLiteral("MonoBleedingEdge"))) {
                detection.isUnity = true;
            }
            if (root.exists(QStringLiteral("Engine"))) {
                detection.isUnreal = true;
            }
        }

        // Scan files for VR libraries, plugins, and executables
        QDirIterator it(installPath,
                        QDir::Dirs | QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        int seen = 0;
        while (it.hasNext() && seen < 3500) {
            it.next();
            ++seen;
            const QFileInfo fi = it.fileInfo();
            const QString rel = root.relativeFilePath(fi.absoluteFilePath());
            const int depth = rel.count(QLatin1Char('/')) + rel.count(QLatin1Char('\\'));
            if (depth > 6)
                continue;

            const QStringList parts = rel.split(QRegularExpression(QStringLiteral("[/\\\\]")),
                                                Qt::SkipEmptyParts);
            bool skip = false;
            for (const QString& part : parts) {
                if (shouldSkipScanDir(part)) {
                    skip = true;
                    break;
                }
            }
            if (skip)
                continue;

            if (fi.isDir()) {
                const QString dirName = fi.fileName();
                if (dirName.endsWith(QStringLiteral("_Data"), Qt::CaseInsensitive))
                    detection.isUnity = true;
                else if (dirName == QLatin1String("Binaries") && fi.dir().dirName() == QLatin1String("Engine"))
                    detection.isUnreal = true;
                else if (dirName.compare(QStringLiteral("SteamVR"), Qt::CaseInsensitive) == 0) {
                    detection.hasVrLibraries = true;
                    if (detection.runtime == VrRuntimeKind::None)
                        detection.runtime = VrRuntimeKind::OpenVR;
                } else if (dirName.compare(QStringLiteral("OpenXR"), Qt::CaseInsensitive) == 0) {
                    detection.hasVrLibraries = true;
                    if (detection.runtime == VrRuntimeKind::None)
                        detection.runtime = VrRuntimeKind::OpenXR;
                } else if (dirName.compare(QStringLiteral("OculusVR"), Qt::CaseInsensitive) == 0) {
                    detection.hasVrLibraries = true;
                    if (detection.runtime == VrRuntimeKind::None)
                        detection.runtime = VrRuntimeKind::Oculus;
                }
                continue;
            }

            const QString fileName = fi.fileName().toLower();
            const QString stem = fi.completeBaseName().toLower();

            if (fileName.endsWith(QStringLiteral(".exe"))) {
                if (isVrExecutableStem(stem))
                    foundVrExecutables.append(fi.absoluteFilePath());
                else
                    foundNonVrExecutables.append(fi.absoluteFilePath());
            }

            if (fileName == QStringLiteral("unityplayer.dll"))
                detection.isUnity = true;

            // OpenVR / SteamVR
            if (fileName == QStringLiteral("openvr_api.dll") || fileName == QStringLiteral("openvr_api64.dll")
                || fileName == QStringLiteral("steamvr.dll") || fileName == QStringLiteral("unityopenvr.dll")
                || fileName == QStringLiteral("unityopenvrenvironment.dll")) {
                detection.hasVrLibraries = true;
                if (detection.runtime == VrRuntimeKind::None) {
                    detection.runtime = VrRuntimeKind::OpenVR;
                    detection.detectedLibrary = fi.fileName();
                }
            }
            // OpenXR
            else if (fileName == QStringLiteral("openxr_loader.dll") || fileName == QStringLiteral("openxr_loader64.dll")
                     || fileName == QStringLiteral("openxr.dll")) {
                detection.hasVrLibraries = true;
                if (detection.runtime == VrRuntimeKind::None || detection.runtime == VrRuntimeKind::OpenVR) {
                    detection.runtime = VrRuntimeKind::OpenXR;
                    detection.detectedLibrary = fi.fileName();
                }
            }
            // Oculus SDK
            else if (fileName == QStringLiteral("ovrplugin.dll") || fileName == QStringLiteral("libovrrt64_1.dll")
                     || fileName == QStringLiteral("libovrrt32_1.dll") || fileName == QStringLiteral("oculusxr.dll")) {
                detection.hasVrLibraries = true;
                if (detection.runtime == VrRuntimeKind::None) {
                    detection.runtime = VrRuntimeKind::Oculus;
                    detection.detectedLibrary = fi.fileName();
                }
            }
            // Unity VR subsystems
            else if (fileName == QStringLiteral("unitysubsystemsmanifest.json")
                     || fileName == QStringLiteral("unityengine.xrmodule.dll")
                     || fileName == QStringLiteral("unityengine.vrmodule.dll")) {
                detection.hasVrLibraries = true;
                detection.isUnity = true;
                if (detection.runtime == VrRuntimeKind::None)
                    detection.runtime = VrRuntimeKind::OpenVR;
            }
        }
    }

    // Determine VrMode
    if (!detection.hasVrLibraries && !genreMentionsVr && !hasVrOption && foundVrExecutables.isEmpty()) {
        detection.vrMode = VrMode::NotVr;
    } else if (genreVrOnly) {
        detection.vrMode = VrMode::VrOnly;
    } else if (hasNonVrOption && hasVrOption) {
        // Options explicitly list both 2D desktop mode and VR mode (e.g. The Forest)
        detection.vrMode = VrMode::VrOptional;
    } else if (!foundVrExecutables.isEmpty() && !foundNonVrExecutables.isEmpty()) {
        // Directory contains both TheForest.exe and TheForestVR.exe
        detection.vrMode = VrMode::VrOptional;
    } else if (genreVrSupport) {
        // Genre explicitly says "VR Support" / "VR Supported" (hybrid)
        detection.vrMode = VrMode::VrOptional;
    } else if (titleIsExplicitVr || (genreMentionsVr && foundNonVrExecutables.isEmpty())) {
        // Title or single executable indicates VR-only
        detection.vrMode = VrMode::VrOnly;
    } else if (detection.hasVrLibraries && !genreMentionsVr) {
        // Found VR DLL in thirdparty/engine, but game does not advertise VR -> treat as optional
        detection.vrMode = VrMode::VrOptional;
    } else {
        detection.vrMode = VrMode::VrOnly;
    }

    if (detection.runtime == VrRuntimeKind::None && detection.vrMode != VrMode::NotVr)
        detection.runtime = VrRuntimeKind::OpenVR;

    // Determine isCurrentLaunchVr
    if (explicitVrInArgs) {
        detection.isCurrentLaunchVr = true;
    } else if (!executablePath.isEmpty()) {
        const QString stem = QFileInfo(executablePath).completeBaseName().toLower();
        const bool exeIsVr = isVrExecutableStem(stem);

        if (exeIsVr) {
            detection.isCurrentLaunchVr = true;
        } else {
            // Executable is standard/desktop (e.g. TheForest.exe)
            if (!selectedOptionId.isEmpty()) {
                bool matchedOpt = false;
                for (const auto& opt : options) {
                    if (opt.id == selectedOptionId) {
                        const bool optIsVr = opt.type.compare(QStringLiteral("vr"), Qt::CaseInsensitive) == 0
                            || opt.title.contains(QStringLiteral("VR"), Qt::CaseInsensitive)
                            || hasExplicitVrArgument(opt.arguments);
                        detection.isCurrentLaunchVr = optIsVr;
                        matchedOpt = true;
                        break;
                    }
                }
                if (!matchedOpt)
                    detection.isCurrentLaunchVr = (detection.vrMode == VrMode::VrOnly);
            } else if (detection.vrMode == VrMode::VrOnly) {
                detection.isCurrentLaunchVr = true;
            } else {
                detection.isCurrentLaunchVr = false;
            }
        }
    } else if (detection.vrMode == VrMode::VrOnly) {
        detection.isCurrentLaunchVr = true;
    } else {
        detection.isCurrentLaunchVr = false;
    }

    // Set suggestedArgs if current launch is VR
    if (detection.isCurrentLaunchVr) {
        if (detection.isUnreal) {
            detection.suggestedArgs = {QStringLiteral("-vr")};
        } else if (detection.isUnity) {
            detection.suggestedArgs = {QStringLiteral("-vrmode"), QStringLiteral("openvr")};
        } else {
            detection.suggestedArgs = {QStringLiteral("-vr")};
        }
    }

    return detection;
}

} // namespace arachnel::core

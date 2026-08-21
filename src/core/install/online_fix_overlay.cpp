#include "online_fix_overlay.h"

#include "steam_shortcut_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSet>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace arachnel::core {
namespace {

constexpr auto kDisabledSuffix = ".arachnel-off";

const QStringList& overlayDllNames()
{
    // FreeTP SteamFix/EpicFix, Ryuu OnlineFix64, and online-fix.me (OnlineFix.dll + overlay).
    static const QStringList names = {
        QStringLiteral("winmm.dll"),
        QStringLiteral("SteamFix64.dll"),
        QStringLiteral("SteamFix32.dll"),
        QStringLiteral("EpicFix64.dll"),
        QStringLiteral("OnlineFix64.dll"),
        QStringLiteral("OnlineFix32.dll"),
        QStringLiteral("OnlineFix.dll"),
        QStringLiteral("SteamOverlay32.dll"),
        QStringLiteral("SteamOverlay64.dll"),
        QStringLiteral("StubDRM32.dll"),
        QStringLiteral("StubDRM64.dll")};
    return names;
}

QString appendDllOverride(QString overrides, const QString& dllStem, const QString& mode)
{
    const QString key = dllStem.toLower();
    if (key.isEmpty() || overrides.contains(key + QLatin1Char('='), Qt::CaseInsensitive))
        return overrides;
    if (!overrides.isEmpty() && !overrides.endsWith(QLatin1Char(';')))
        overrides += QLatin1Char(';');
    return overrides + key + mode;
}

QString findSteamidraCmdStub()
{
    const QStringList roots = {
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation),
    };
    QSet<QString> seen;
    for (const QString& root : roots) {
        if (root.isEmpty() || seen.contains(root))
            continue;
        seen.insert(root);
        const QStringList candidates = {
            root + QStringLiteral("/plugins/steamidra/online_fix_kit/cmd_stub.exe"),
            root + QStringLiteral("/Arachnel/plugins/steamidra/online_fix_kit/cmd_stub.exe"),
        };
        for (const QString& path : candidates) {
            if (QFileInfo::exists(path))
                return path;
        }
    }
    return {};
}

void plantFreetpPromoBlock(const QString& dir)
{
    if (dir.isEmpty() || !QDir(dir).exists())
        return;

    QDir().mkpath(dir + QStringLiteral("/FreeTP/UserData"));
    const QString anon = dir + QStringLiteral("/FreeTP/UserData/AnonFolderSave.txt");
    if (!QFileInfo::exists(anon)) {
        QFile f(anon);
        if (f.open(QIODevice::WriteOnly))
            f.close();
    }

    const QString stubSrc = findSteamidraCmdStub();
    if (stubSrc.isEmpty())
        return;
    const QString stubDst = dir + QStringLiteral("/cmd.exe");
    const QFileInfo srcInfo(stubSrc);
    const QFileInfo dstInfo(stubDst);
    if (dstInfo.exists() && dstInfo.size() == srcInfo.size()
        && dstInfo.lastModified() >= srcInfo.lastModified())
        return;
    QFile::remove(stubDst);
    QFile::copy(stubSrc, stubDst);
}

QString readDllListOverrides(const QString& listPath)
{
    QFile file(listPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QString overrides;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (!line.endsWith(QStringLiteral(".dll"), Qt::CaseInsensitive))
            continue;
        const QString stem = QFileInfo(line).completeBaseName().toLower();
        overrides = appendDllOverride(overrides, stem, QStringLiteral("=n,b"));
    }
    return overrides;
}

QString readIniAppId(const QString& iniPath, const QString& key)
{
    QFile file(iniPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QRegularExpression re(QStringLiteral("%1\\s*=\\s*(\\d+)").arg(key),
                                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(QString::fromUtf8(file.readAll()));
    return match.hasMatch() ? match.captured(1) : QString();
}

QString buildOverlayWineDllOverrides(const QString& overlayDir)
{
    // SOFL default Online-Fix overrides (=n,b). Scanned game DLLs are merged on top.
    QString overrides = QStringLiteral(
        "d3d11=n;d3d10=n;d3d10core=n;dxgi=n;openvr_api_dxvk=n;d3d12=n;d3d12core=n;d3d9=n;d3d8=n;"
        "onlinefix64=n,b;onlinefix=n,b;steamoverlay64=n,b;steamoverlay32=n,b;"
        "winmm=n,b;dnet=n,b;steam_api64=n,b;steam_api=n,b;"
        "winhttp=n,b;steamfix64=n,b;steamfix32=n,b;epicfix64=n,b");

    const QDir dir(overlayDir);
    if (!dir.exists())
        return overrides;

    for (const QString& listName :
         {QStringLiteral("winmm.txt"), QStringLiteral("dlllist.txt")}) {
        const QString listPath = dir.filePath(listName);
        if (QFileInfo::exists(listPath)) {
            for (const QString& token :
                 readDllListOverrides(listPath).split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
                const QString stem = token.section(QLatin1Char('='), 0, 0).trimmed();
                const QString mode = QLatin1Char('=') + token.section(QLatin1Char('='), 1).trimmed();
                if (!stem.isEmpty())
                    overrides = appendDllOverride(overrides, stem, mode);
            }
        }
    }

    const QStringList dlls = dir.entryList({QStringLiteral("*.dll")}, QDir::Files);
    for (const QString& name : dlls) {
        const QString lower = name.toLower();
        const QString stem = QFileInfo(name).completeBaseName().toLower();
        if (lower.startsWith(QStringLiteral("win")) || lower.contains(QStringLiteral("fix"))
            || lower.contains(QStringLiteral("overlay")) || lower.startsWith(QStringLiteral("steam"))
            || lower.startsWith(QStringLiteral("online")) || lower.startsWith(QStringLiteral("epic"))
            || lower.startsWith(QStringLiteral("custom")) || lower.startsWith(QStringLiteral("dnet"))
            || lower.startsWith(QStringLiteral("emp")))
            overrides = appendDllOverride(overrides, stem, QStringLiteral("=n,b"));
    }
    return overrides;
}

#if defined(Q_OS_LINUX)
QString steamClientRoot()
{
    // Prefer a root that actually has Steam client libs (Debian/.steam often wins over
    // an empty ~/.local/share/Steam that only holds GE-Proton tools).
    const QString home = QDir::homePath();
    const QStringList candidates = {
        home + QStringLiteral("/.steam/steam"),
        home + QStringLiteral("/.steam/root"),
        home + QStringLiteral("/.local/share/Steam"),
        home + QStringLiteral("/.var/app/com.valvesoftware.Steam/data/Steam"),
        home + QStringLiteral("/.var/app/com.valvesoftware.Steam/.local/share/Steam"),
    };
    QString fallback;
    for (const QString& candidate : candidates) {
        if (!QDir(candidate).exists())
            continue;
        if (fallback.isEmpty())
            fallback = candidate;
        if (QFileInfo::exists(candidate + QStringLiteral("/ubuntu12_64/gameoverlayrenderer.so"))
            || QFileInfo::exists(candidate + QStringLiteral("/steam.sh"))
            || QFileInfo::exists(candidate + QStringLiteral("/ubuntu12_32/steam")))
            return QFileInfo(candidate).canonicalFilePath().isEmpty()
                       ? candidate
                       : QFileInfo(candidate).canonicalFilePath();
    }
    return fallback.isEmpty() ? home + QStringLiteral("/.local/share/Steam") : fallback;
}

QString gameOverlayPreloadPaths()
{
    // SOFL: LD_PRELOAD both 32/64 gameoverlayrenderer from the real Steam install.
    QStringList soPaths;
    const QString home = QDir::homePath();
    const QStringList roots = {
        steamClientRoot(),
        home + QStringLiteral("/.steam/steam"),
        home + QStringLiteral("/.steam/debian-installation"),
        home + QStringLiteral("/.local/share/Steam"),
    };
    for (const QString& root : roots) {
        if (root.isEmpty() || !QDir(root).exists())
            continue;
        const QString so32 = root + QStringLiteral("/ubuntu12_32/gameoverlayrenderer.so");
        const QString so64 = root + QStringLiteral("/ubuntu12_64/gameoverlayrenderer.so");
        if (QFileInfo::exists(so32) && !soPaths.contains(so32))
            soPaths.append(so32);
        if (QFileInfo::exists(so64) && !soPaths.contains(so64))
            soPaths.append(so64);
        if (soPaths.size() >= 2)
            break;
    }
    if (soPaths.isEmpty()) {
        const QString root = steamClientRoot();
        soPaths = {root + QStringLiteral("/ubuntu12_32/gameoverlayrenderer.so"),
                   root + QStringLiteral("/ubuntu12_64/gameoverlayrenderer.so")};
    }
    return soPaths.join(QLatin1Char(':'));
}

bool dirHasBundledSteamOverlayDll(const QString& dir)
{
    const QDir d(dir);
    return d.exists(QStringLiteral("SteamOverlay32.dll"))
        || d.exists(QStringLiteral("SteamOverlay64.dll"));
}

void appendSteamOverlayEnvironment(LaunchInfo* info, const QString& fakeSteamId,
                                   const QString& overlayDir)
{
    const QString overlayId = fakeSteamId.isEmpty() ? QStringLiteral("480") : fakeSteamId;
    // SpaceWar AppId is still required for SteamFix / OF.me IPC.
    info->environmentExtras.insert(QStringLiteral("SteamAppId"), overlayId);
    info->environmentExtras.insert(QStringLiteral("SteamGameId"), overlayId);
    info->environmentExtras.insert(QStringLiteral("SteamOverlayGameId"), overlayId);

    // OF.me ships SteamOverlay32/64 next to the game. Forcing Valve's
    // gameoverlayrenderer on top makes Steam show "Failed to load steam overlay
    // dll" (126) on 32-bit titles and can trip OF.me self-protection.
    if (dirHasBundledSteamOverlayDll(overlayDir)
        || QDir(overlayDir).exists(QStringLiteral("OnlineFix.dll"))
        || QDir(overlayDir).exists(QStringLiteral("OnlineFix64.dll"))) {
        // Explicit clear so host/Steam LD_PRELOAD cannot leak into Proton.
        info->environmentExtras.insert(QStringLiteral("LD_PRELOAD"), QString());
        info->environmentExtras.insert(QStringLiteral("ENABLE_VK_LAYER_VALVE_steam_overlay_1"),
                                       QStringLiteral("0"));
        return;
    }

    const QString preload = gameOverlayPreloadPaths();
    const QString existing = info->environmentExtras.value(QStringLiteral("LD_PRELOAD"));
    info->environmentExtras.insert(QStringLiteral("LD_PRELOAD"),
                                   existing.isEmpty() ? preload : existing + QLatin1Char(':') + preload);
    info->environmentExtras.insert(QStringLiteral("ENABLE_VK_LAYER_VALVE_steam_overlay_1"),
                                   QStringLiteral("true"));
}
#endif

bool dirHasActiveOverlay(const QDir& dir)
{
    for (const QString& name : overlayDllNames()) {
        if (dir.exists(name))
            return true;
    }
    return dir.exists(QStringLiteral("SteamFix.ini")) || dir.exists(QStringLiteral("OnlineFix.ini"))
        || dir.exists(QStringLiteral("winmm.txt")) || dir.exists(QStringLiteral("dlllist.txt"));
}

bool dirHasDisabledOverlay(const QDir& dir)
{
    for (const QString& name : overlayDllNames()) {
        if (dir.exists(name + QLatin1String(kDisabledSuffix)))
            return true;
    }
    return false;
}

bool dirLooksLikeOverlay(const QDir& dir)
{
    return dirHasActiveOverlay(dir) || dirHasDisabledOverlay(dir);
}

bool hasValveBackup(const QString& installPath)
{
    QDirIterator it(installPath,
                    {QStringLiteral("steam_api64.dll.arachnel-valve"),
                     QStringLiteral("steam_api.dll.arachnel-valve")},
                    QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext();
}

bool hasSteamSettings(const QString& installPath)
{
    QDirIterator it(installPath, {QStringLiteral("steam_settings")}, QDir::Dirs,
                    QDirIterator::Subdirectories);
    int n = 0;
    while (it.hasNext() && n < 2000) {
        it.next();
        ++n;
        if (QFileInfo::exists(it.filePath() + QStringLiteral("/DLC.txt"))
            || QFileInfo::exists(it.filePath() + QStringLiteral("/steam_interfaces.txt")))
            return true;
    }
    return false;
}

QJsonObject readOnlineFixMarker(const QString& installPath)
{
    QFile file(installPath + QStringLiteral("/.arachnel-steamidra"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("onlineFix")).toObject();
}

bool restoreValveSteamApiFiles(const QString& installPath)
{
    bool wrote = false;
    QDirIterator it(installPath,
                    {QStringLiteral("steam_api64.dll.arachnel-valve"),
                     QStringLiteral("steam_api.dll.arachnel-valve")},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString bak = it.filePath();
        QString api = bak;
        api.chop(QStringLiteral(".arachnel-valve").size());
        if (QFileInfo::exists(api))
            QFile::remove(api);
        if (QFile::copy(bak, api))
            wrote = true;
    }
    return wrote;
}

bool shouldSkipOverlayScanDir(const QString& name)
{
    const QString lower = name.toLower();
    return lower == QLatin1String(".depotdownloader") || lower == QLatin1String("staging")
        || lower == QLatin1String("engine") || lower == QLatin1String("intermediate")
        || lower == QLatin1String("saved") || lower == QLatin1String("thirdparty");
}

QStringList findOverlayDirs(const QString& installPath)
{
    QStringList out;
    if (installPath.isEmpty() || !QFileInfo::exists(installPath))
        return out;

    const QDir root(installPath);
    if (dirLooksLikeOverlay(root))
        out.append(root.absolutePath());

    // UE shipping + shallow scan for renamed overlays (depth-limited).
    QDirIterator it(installPath,
                    QDir::Dirs | QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    int seen = 0;
    while (it.hasNext() && seen < 4000) {
        it.next();
        ++seen;
        const QFileInfo fi = it.fileInfo();
        const QString rel = root.relativeFilePath(fi.absoluteFilePath());
        const int depth = rel.count(QLatin1Char('/')) + rel.count(QLatin1Char('\\'));
        if (depth > 6)
            continue;
        // Skip known non-game trees by path segment.
        const QStringList parts = rel.split(QRegularExpression(QStringLiteral("[/\\\\]")),
                                            Qt::SkipEmptyParts);
        bool skip = false;
        for (const QString& part : parts) {
            if (shouldSkipOverlayScanDir(part)) {
                skip = true;
                break;
            }
        }
        if (skip)
            continue;
        if (fi.isDir()) {
            const QString name = fi.fileName();
            if ((name == QLatin1String("Win64") || name == QLatin1String("Win32"))
                && fi.dir().dirName() == QLatin1String("Binaries")) {
                const QDir shipping(fi.absoluteFilePath());
                if (dirLooksLikeOverlay(shipping)
                    && !out.contains(shipping.absolutePath(), Qt::CaseInsensitive))
                    out.append(shipping.absolutePath());
            }
            continue;
        }
        const QString fileName = fi.fileName();
        const bool match = fileName == QLatin1String("winmm.dll")
            || fileName == QLatin1String("SteamFix64.dll")
            || fileName == QLatin1String("SteamFix32.dll")
            || fileName == QLatin1String("OnlineFix64.dll")
            || fileName == QLatin1String("OnlineFix.dll")
            || fileName == QLatin1String("OnlineFix32.dll")
            || fileName == QLatin1String("SteamOverlay32.dll")
            || fileName == QLatin1String("SteamOverlay64.dll")
            || fileName == QLatin1String("StubDRM32.dll")
            || fileName == QLatin1String("SteamFix.ini")
            || fileName == QLatin1String("OnlineFix.ini")
            || fileName == QLatin1String("dlllist.txt")
            || fileName == QLatin1String("winmm.txt")
            || fileName.endsWith(QLatin1String(kDisabledSuffix));
        if (!match)
            continue;
        const QDir parent = fi.dir();
        if (dirLooksLikeOverlay(parent)
            && !out.contains(parent.absolutePath(), Qt::CaseInsensitive))
            out.append(parent.absolutePath());
    }
    return out;
}

bool renameOverlayInDir(const QDir& dir, bool enable, QString* error)
{
    bool touched = false;
    for (const QString& name : overlayDllNames()) {
        const QString active = dir.filePath(name);
        const QString disabled = active + QLatin1String(kDisabledSuffix);
        if (enable) {
            if (!QFileInfo::exists(disabled))
                continue;
            if (QFileInfo::exists(active))
                QFile::remove(active);
            if (!QFile::rename(disabled, active)) {
                if (error)
                    *error = QCoreApplication::translate("Core", "Failed to enable Online Fix: %1")
                                 .arg(active);
                return false;
            }
            touched = true;
        } else {
            if (!QFileInfo::exists(active))
                continue;
            if (QFileInfo::exists(disabled))
                QFile::remove(disabled);
            if (!QFile::rename(active, disabled)) {
                if (error)
                    *error = QCoreApplication::translate("Core", "Failed to disable Online Fix: %1")
                                 .arg(active);
                return false;
            }
            touched = true;
        }
    }
    // steam_appid.txt FakeAppId fallback (32-bit / no matching winmm).
    const QString appIdFile = dir.filePath(QStringLiteral("steam_appid.txt"));
    const QString appIdOff = appIdFile + QLatin1String(kDisabledSuffix);
    if (enable) {
        if (QFileInfo::exists(appIdOff)) {
            if (QFileInfo::exists(appIdFile))
                QFile::remove(appIdFile);
            QFile::rename(appIdOff, appIdFile);
            touched = true;
        }
    } else if (QFileInfo::exists(appIdFile)) {
        if (QFileInfo::exists(appIdOff))
            QFile::remove(appIdOff);
        QFile::rename(appIdFile, appIdOff);
        touched = true;
    }
    Q_UNUSED(touched);
    return true;
}

void updateMarkerEnabled(const QString& installPath, bool enabled)
{
    const QString markerPath = installPath + QStringLiteral("/.arachnel-steamidra");
    QJsonObject root;
    QFile file(markerPath);
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }
    QJsonObject onlineFix = root.value(QStringLiteral("onlineFix")).toObject();
    onlineFix.insert(QStringLiteral("enabled"), enabled);
    onlineFix.insert(QStringLiteral("embedded"), true);
    // Drop stale "goldberg" from older builds - SteamFix/winmm is the only overlay now.
    onlineFix.remove(QStringLiteral("backend"));
    root.insert(QStringLiteral("onlineFix"), onlineFix);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

} // namespace

bool isSteamClientRunning()
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
            const QString name = QString::fromWCharArray(entry.szExeFile);
            if (name.compare(QStringLiteral("steam.exe"), Qt::CaseInsensitive) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return found;
#else
    QProcess process;
    process.start(QStringLiteral("pidof"), {QStringLiteral("steam")});
    if (process.waitForFinished(3000) && process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0 && !process.readAllStandardOutput().trimmed().isEmpty())
        return true;
    process.start(QStringLiteral("pgrep"), {QStringLiteral("-x"), QStringLiteral("steam")});
    return process.waitForFinished(3000) && process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0;
#endif
}

bool tryStartSteamClient()
{
#if defined(Q_OS_WIN)
    QStringList candidates;
    const QString install = findSteamInstallPath();
    if (!install.isEmpty())
        candidates.append(install + QStringLiteral("/steam.exe"));
    candidates.append({
        QStringLiteral("C:/Program Files (x86)/Steam/steam.exe"),
        QStringLiteral("C:/Program Files/Steam/steam.exe"),
    });
    for (const QString& path : candidates) {
        if (!QFileInfo::exists(path))
            continue;
        qint64 pid = 0;
        if (QProcess::startDetached(path, {}, QFileInfo(path).absolutePath(), &pid))
            return true;
    }
    return false;
#else
    const QStringList candidates = {
        QStringLiteral("steam"),
        QDir::homePath() + QStringLiteral("/.local/share/Steam/steam.sh"),
        QDir::homePath() + QStringLiteral("/.steam/steam/steam.sh"),
        QStringLiteral("/usr/bin/steam"),
    };
    for (const QString& cmd : candidates) {
        if (cmd != QStringLiteral("steam") && !QFileInfo::exists(cmd))
            continue;
        // Detach with a clean env so host Steam/Nix sessions do not poison
        // /usr/bin/env inside steam.sh via LD_LIBRARY_PATH from steam-runtime.
        QProcess process;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove(QStringLiteral("LD_LIBRARY_PATH"));
        env.remove(QStringLiteral("LD_PRELOAD"));
        env.remove(QStringLiteral("STEAM_RUNTIME"));
        env.remove(QStringLiteral("STEAM_RUNTIME_LIBRARY_PATH"));
        process.setProcessEnvironment(env);
        process.setProgram(cmd);
        process.setArguments({});
        qint64 pid = 0;
        if (process.startDetached(&pid))
            return true;
    }
    return false;
#endif
}

OnlineFixOverlayState detectOnlineFixOverlay(const QString& installPath)
{
    OnlineFixOverlayState state;
    if (installPath.isEmpty() || !QFileInfo::exists(installPath))
        return state;

    // Prefer SteamFix / winmm overlay dirs over older valve-backup heuristics.
    const QStringList dirs = findOverlayDirs(installPath);
    if (!dirs.isEmpty()) {
        state.present = true;
        state.overlayDir = dirs.first();
        for (const QString& path : dirs) {
            const QDir dir(path);
            bool hasActiveDll = false;
            for (const QString& name : overlayDllNames()) {
                if (dir.exists(name)) {
                    hasActiveDll = true;
                    break;
                }
            }
            const bool hasDisabled = dirHasDisabledOverlay(dir);
            // Default on: live DLLs, or FakeAppId mode (ini/txt + steam_appid) when nothing was
            // renamed off. Disable renames both DLLs and steam_appid.txt.
            const bool fakeAppIdMode = dir.exists(QStringLiteral("steam_appid.txt"))
                && (dir.exists(QStringLiteral("SteamFix.ini"))
                    || dir.exists(QStringLiteral("OnlineFix.ini"))
                    || dir.exists(QStringLiteral("winmm.txt")));
            if (hasActiveDll || (fakeAppIdMode && !hasDisabled)) {
                state.enabled = true;
                state.overlayDir = path;
                break;
            }
        }
        if (!state.enabled) {
            for (const QString& path : dirs) {
                if (dirHasDisabledOverlay(QDir(path))) {
                    state.overlayDir = path;
                    break;
                }
            }
        }
        return state;
    }

    // Old embeds: valve steam_api backups and/or marker only.
    const QJsonObject of = readOnlineFixMarker(installPath);
    if (hasValveBackup(installPath) || of.value(QStringLiteral("embedded")).toBool(false)
        || of.contains(QStringLiteral("enabled"))) {
        state.present = true;
        state.overlayDir = installPath;
        if (of.contains(QStringLiteral("enabled")))
            state.enabled = of.value(QStringLiteral("enabled")).toBool(false);
        else
            state.enabled = hasSteamSettings(installPath) && hasValveBackup(installPath);
    }
    return state;
}

bool setOnlineFixOverlayEnabled(const QString& installPath, bool enabled, QString* error)
{
    if (installPath.isEmpty() || !QFileInfo::exists(installPath)) {
        if (error)
            *error = QCoreApplication::translate("Core", "Online Fix overlay not found in this install");
        return false;
    }

    const QStringList dirs = findOverlayDirs(installPath);
    if (!dirs.isEmpty()) {
        for (const QString& path : dirs) {
            if (!renameOverlayInDir(QDir(path), enabled, error))
                return false;
        }
        updateMarkerEnabled(installPath, enabled);
        return true;
    }

    // No SteamFix overlay yet - marker toggle; restore Valve DLLs when disabling old embeds.
    if (!enabled)
        restoreValveSteamApiFiles(installPath);
    updateMarkerEnabled(installPath, enabled);
    return true;
}

QVariantMap onlineFixOverlayInfo(const QString& installPath)
{
    const OnlineFixOverlayState state = detectOnlineFixOverlay(installPath);
    QString label;
    if (!state.present)
        label = QCoreApplication::translate("Core", "Not installed");
    else if (state.enabled)
        label = QCoreApplication::translate("Core", "Enabled");
    else
        label = QCoreApplication::translate("Core", "Disabled");

    return {
        {QStringLiteral("onlineFixPresent"), state.present},
        {QStringLiteral("onlineFixEnabled"), state.enabled},
        {QStringLiteral("onlineFixCanToggle"), state.present},
        {QStringLiteral("onlineFixLabel"), label},
        {QStringLiteral("onlineFixOverlayDir"), state.overlayDir},
    };
}

void applyOnlineFixLaunchInfo(const QString& installPath, LaunchInfo* info)
{
    if (!info || installPath.isEmpty())
        return;

    OnlineFixOverlayState state = detectOnlineFixOverlay(installPath);
    if (!state.enabled && !info->workingDirectory.isEmpty()
        && info->workingDirectory != installPath) {
        const OnlineFixOverlayState wd = detectOnlineFixOverlay(info->workingDirectory);
        if (wd.enabled)
            state = wd;
    }
    if (!state.enabled)
        return;

    QString overlayDir = state.overlayDir;
    if (overlayDir.isEmpty())
        overlayDir = info->workingDirectory.isEmpty() ? installPath : info->workingDirectory;

    QString overrides = buildOverlayWineDllOverrides(overlayDir);
    if (overrides.isEmpty()) {
        overrides = QStringLiteral(
            "onlinefix64=n,b;onlinefix=n,b;steamoverlay64=n,b;steamoverlay32=n,b;"
            "winmm=n,b;dnet=n,b;steam_api64=n,b;steam_api=n,b;winhttp=n,b");
    }

    if (info->wineDllOverrides.trimmed().isEmpty()) {
        info->wineDllOverrides = overrides;
    } else {
        for (const QString& token : overrides.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
            const QString stem = token.section(QLatin1Char('='), 0, 0).trimmed();
            const QString mode = QLatin1Char('=') + token.section(QLatin1Char('='), 1).trimmed();
            if (!stem.isEmpty())
                info->wineDllOverrides = appendDllOverride(info->wineDllOverrides, stem, mode);
        }
    }

    // Proton + WINEDLLOVERRIDES is enough for Online Fix. Do not wrap in legacy
    // steam-runtime/run.sh: that helper often exits after fork, so the launcher
    // thinks the game died immediately while Wine may still be starting.
    info->environmentExtras.remove(QStringLiteral("ARACHNEL_USE_STEAM_RUNTIME"));

    QString fakeAppId = QStringLiteral("480");
    const QString steamFixIni = QDir(overlayDir).filePath(QStringLiteral("SteamFix.ini"));
    const QString onlineFixIni = QDir(overlayDir).filePath(QStringLiteral("OnlineFix.ini"));
    if (QFileInfo::exists(steamFixIni)) {
        const QString fromIni = readIniAppId(steamFixIni, QStringLiteral("FakeAppId"));
        if (!fromIni.isEmpty())
            fakeAppId = fromIni;
    } else if (QFileInfo::exists(onlineFixIni)) {
        const QString fromIni = readIniAppId(onlineFixIni, QStringLiteral("FakeAppId"));
        if (!fromIni.isEmpty())
            fakeAppId = fromIni;
    }

    // steam_appid.txt is read by steam_api / Steamworks.NET. Write on every OS -
    // critical for 32-bit Unity installs where only SteamFix64+x64 winmm was embedded
    // (wrong-arch winmm in the game folder fails LoadLibrary with no SysWOW64 fallback).
    auto ensureSteamAppIdFile = [&fakeAppId](const QString& dir) {
        if (dir.isEmpty() || !QDir(dir).exists())
            return;
        const QString appIdFile = QDir(dir).filePath(QStringLiteral("steam_appid.txt"));
        QFile out(appIdFile);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return;
        out.write(fakeAppId.toUtf8());
        if (!fakeAppId.endsWith(QLatin1Char('\n')))
            out.write("\n");
    };
    ensureSteamAppIdFile(overlayDir);
    if (!info->workingDirectory.isEmpty()
        && QFileInfo(info->workingDirectory).absoluteFilePath()
            != QFileInfo(overlayDir).absoluteFilePath())
        ensureSteamAppIdFile(info->workingDirectory);
    if (!info->executable.isEmpty()) {
        const QString exeDir = QFileInfo(info->executable).absolutePath();
        if (QFileInfo(exeDir).absoluteFilePath() != QFileInfo(overlayDir).absoluteFilePath())
            ensureSteamAppIdFile(exeDir);
    }

    // Windows + Proton: block FreeTP promo browser (path-local .hash / noop cmd.exe).
    plantFreetpPromoBlock(overlayDir);
    if (!info->workingDirectory.isEmpty())
        plantFreetpPromoBlock(info->workingDirectory);
    if (!info->executable.isEmpty())
        plantFreetpPromoBlock(QFileInfo(info->executable).absolutePath());

    // Proton Steam client LoadLibrary("GameOverlayRenderer.dll") for Spacewar - many
    // prefixes only have the x64 renderer. Alias FreeTP SteamOverlay* to that name.
    // Do NOT plant the alias next to OF.me: an extra GameOverlayRenderer.dll trips
    // Self-protection (error 4). Valve overlay is already disabled for OF.me above.
    auto plantOverlayAlias = [](const QString& dir) {
        if (dir.isEmpty() || !QDir(dir).exists())
            return;
        const QString ov32 = dir + QStringLiteral("/SteamOverlay32.dll");
        const QString ov64 = dir + QStringLiteral("/SteamOverlay64.dll");
        auto copyAlias = [](const QString& src, const QString& dst) {
            if (!QFileInfo::exists(src))
                return;
            if (QFileInfo::exists(dst))
                QFile::remove(dst);
            QFile::copy(src, dst);
        };
        if (QFileInfo::exists(ov32))
            copyAlias(ov32, dir + QStringLiteral("/GameOverlayRenderer.dll"));
        if (QFileInfo::exists(ov64))
            copyAlias(ov64, dir + QStringLiteral("/GameOverlayRenderer64.dll"));
    };
    auto stripOfMeOverlayAlias = [](const QString& dir) {
        if (dir.isEmpty() || !QDir(dir).exists())
            return;
        QFile::remove(dir + QStringLiteral("/GameOverlayRenderer.dll"));
        QFile::remove(dir + QStringLiteral("/GameOverlayRenderer64.dll"));
    };
    const QDir ovDir(overlayDir);
    const bool onlineFixMe = ovDir.exists(QStringLiteral("OnlineFix.ini"))
                             || ovDir.exists(QStringLiteral("OnlineFix.dll"))
                             || ovDir.exists(QStringLiteral("OnlineFix64.dll"));
    auto applyAliasPolicy = [&](const QString& dir) {
        if (onlineFixMe)
            stripOfMeOverlayAlias(dir);
        else
            plantOverlayAlias(dir);
    };
    applyAliasPolicy(overlayDir);
    if (!info->workingDirectory.isEmpty())
        applyAliasPolicy(info->workingDirectory);
    if (!info->executable.isEmpty())
        applyAliasPolicy(QFileInfo(info->executable).absolutePath());

#if defined(Q_OS_LINUX)
    // Valve LD_PRELOAD only when the install does not already ship OF.me overlay DLLs.
    appendSteamOverlayEnvironment(info, fakeAppId, overlayDir);
#endif
}

} // namespace arachnel::core

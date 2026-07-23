#include "online_fix_overlay.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>

namespace arachnel::core {
namespace {

constexpr auto kDisabledSuffix = ".arachnel-off";

const QStringList& overlayDllNames()
{
    static const QStringList names = {QStringLiteral("winmm.dll"), QStringLiteral("SteamFix64.dll"),
                                      QStringLiteral("SteamFix32.dll"), QStringLiteral("EpicFix64.dll")};
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
        "onlinefix64=n,b;steamoverlay64=n,b;winmm=n,b;dnet=n,b;steam_api64=n,b;steam_api=n,b;"
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
    const QString home = QDir::homePath();
    const QStringList candidates = {
        home + QStringLiteral("/.local/share/Steam"),
        home + QStringLiteral("/.steam/steam"),
        home + QStringLiteral("/.steam/root"),
        home + QStringLiteral("/.var/app/com.valvesoftware.Steam/data/Steam"),
        home + QStringLiteral("/.var/app/com.valvesoftware.Steam/.local/share/Steam"),
    };
    for (const QString& candidate : candidates) {
        if (QDir(candidate).exists())
            return candidate;
    }
    return home + QStringLiteral("/.local/share/Steam");
}

QString gameOverlayPreloadPaths()
{
    // SOFL hardcodes ~/.local/share/Steam; also try other Steam roots if .so lives there.
    QStringList soPaths;
    const QString home = QDir::homePath();
    const QStringList roots = {
        home + QStringLiteral("/.local/share/Steam"),
        steamClientRoot(),
        home + QStringLiteral("/.steam/steam"),
    };
    for (const QString& root : roots) {
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

void appendSteamOverlayEnvironment(LaunchInfo* info, const QString& fakeSteamId)
{
    const QString preload = gameOverlayPreloadPaths();
    const QString existing = info->environmentExtras.value(QStringLiteral("LD_PRELOAD"));
    info->environmentExtras.insert(QStringLiteral("LD_PRELOAD"),
                                   existing.isEmpty() ? preload : existing + QLatin1Char(':') + preload);
    info->environmentExtras.insert(QStringLiteral("ENABLE_VK_LAYER_VALVE_steam_overlay_1"),
                                   QStringLiteral("true"));
    const QString overlayId = fakeSteamId.isEmpty() ? QStringLiteral("480") : fakeSteamId;
    info->environmentExtras.insert(QStringLiteral("SteamOverlayGameId"), overlayId);
}
#endif

bool dirHasActiveOverlay(const QDir& dir)
{
    for (const QString& name : overlayDllNames()) {
        if (dir.exists(name))
            return true;
    }
    return dir.exists(QStringLiteral("SteamFix.ini")) || dir.exists(QStringLiteral("winmm.txt"));
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
            || fileName == QLatin1String("SteamFix.ini")
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
    Q_UNUSED(touched);
    return true;
}

void updateMarkerEnabled(const QString& installPath, bool enabled)
{
    const QString markerPath = installPath + QStringLiteral("/.arachnel-steamidra");
    if (!QFileInfo::exists(markerPath))
        return;
    QFile file(markerPath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    QJsonObject onlineFix = root.value(QStringLiteral("onlineFix")).toObject();
    onlineFix.insert(QStringLiteral("enabled"), enabled);
    onlineFix.insert(QStringLiteral("embedded"), true);
    root.insert(QStringLiteral("onlineFix"), onlineFix);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

} // namespace

#if defined(Q_OS_LINUX)
bool isSteamClientRunning()
{
    QProcess process;
    process.start(QStringLiteral("pidof"), {QStringLiteral("steam")});
    if (process.waitForFinished(3000) && process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0 && !process.readAllStandardOutput().trimmed().isEmpty())
        return true;
    process.start(QStringLiteral("pgrep"), {QStringLiteral("-x"), QStringLiteral("steam")});
    return process.waitForFinished(3000) && process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0;
}

bool tryStartSteamClient()
{
    const QStringList candidates = {
        QStringLiteral("steam"),
        QDir::homePath() + QStringLiteral("/.local/share/Steam/steam.sh"),
        QDir::homePath() + QStringLiteral("/.steam/steam/steam.sh"),
        QStringLiteral("/usr/bin/steam"),
    };
    for (const QString& cmd : candidates) {
        if (cmd != QStringLiteral("steam") && !QFileInfo::exists(cmd))
            continue;
        if (QProcess::startDetached(cmd, {}))
            return true;
    }
    return false;
}
#endif

OnlineFixOverlayState detectOnlineFixOverlay(const QString& installPath)
{
    OnlineFixOverlayState state;
    const QStringList dirs = findOverlayDirs(installPath);
    if (dirs.isEmpty())
        return state;

    state.present = true;
    state.overlayDir = dirs.first();
    for (const QString& path : dirs) {
        const QDir dir(path);
        if (dir.exists(QStringLiteral("winmm.dll")) || dir.exists(QStringLiteral("SteamFix64.dll"))
            || dir.exists(QStringLiteral("SteamFix32.dll"))) {
            state.enabled = true;
            state.overlayDir = path;
            break;
        }
    }
    if (!state.enabled) {
        // Disabled-only: still present.
        for (const QString& path : dirs) {
            if (dirHasDisabledOverlay(QDir(path))) {
                state.overlayDir = path;
                break;
            }
        }
    }
    return state;
}

bool setOnlineFixOverlayEnabled(const QString& installPath, bool enabled, QString* error)
{
    const QStringList dirs = findOverlayDirs(installPath);
    if (dirs.isEmpty()) {
        if (error)
            *error = QCoreApplication::translate("Core", "Online Fix overlay not found in this install");
        return false;
    }
    for (const QString& path : dirs) {
        if (!renameOverlayInDir(QDir(path), enabled, error))
            return false;
    }
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
            "onlinefix64=n,b;steamoverlay64=n,b;winmm=n,b;dnet=n,b;steam_api64=n,b;winhttp=n,b");
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

    info->environmentExtras.insert(QStringLiteral("ARACHNEL_USE_STEAM_RUNTIME"),
                                   QStringLiteral("1"));

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

#if defined(Q_OS_LINUX)
    if (!info->environmentExtras.contains(QStringLiteral("SteamOverlayGameId"))
        && isSteamClientRunning()) {
        appendSteamOverlayEnvironment(info, fakeAppId);
    }
#else
    Q_UNUSED(fakeAppId);
#endif
}

} // namespace arachnel::core

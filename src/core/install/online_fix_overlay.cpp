#include "online_fix_overlay.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
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

} // namespace arachnel::core

#include "steamworks_install_scripts.h"

#include "installscript_vdf.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>

namespace arachnel::core {
namespace {

QString normalizePath(QString path)
{
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    return path;
}

QStringList candidateSteamRoots()
{
    QStringList roots;
    const auto appendIfExists = [&](const QString& path) {
        const QString n = normalizePath(path);
        if (!n.isEmpty() && QDir(n).exists() && !roots.contains(n))
            roots.append(n);
    };

#if defined(Q_OS_LINUX)
    const QString home = QDir::homePath();
    appendIfExists(home + QStringLiteral("/.local/share/Steam"));
    appendIfExists(home + QStringLiteral("/.steam/steam"));
    appendIfExists(home + QStringLiteral("/.steam/root"));
    appendIfExists(home + QStringLiteral("/.var/app/com.valvesoftware.Steam/data/Steam"));
    appendIfExists(QStringLiteral("/usr/share/steam"));
#elif defined(Q_OS_WIN)
    appendIfExists(QStringLiteral("C:/Program Files (x86)/Steam"));
    appendIfExists(QStringLiteral("C:/Program Files/Steam"));
#endif
    return roots;
}

QStringList libraryRootsForSteam(const QString& steamRoot)
{
    QStringList libs;
    const QString root = normalizePath(steamRoot);
    libs.append(root);

    const QStringList vdfPaths = {
        root + QStringLiteral("/steamapps/libraryfolders.vdf"),
        root + QStringLiteral("/config/libraryfolders.vdf"),
    };
    const QRegularExpression pathRe(R"re("path"\s+"([^"]+)")re",
                                    QRegularExpression::CaseInsensitiveOption);
    for (const QString& vdfPath : vdfPaths) {
        QFile file(vdfPath);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QString text = QString::fromUtf8(file.readAll());
        auto it = pathRe.globalMatch(text);
        while (it.hasNext()) {
            const QString lib = normalizePath(it.next().captured(1));
            if (!lib.isEmpty() && QDir(lib).exists() && !libs.contains(lib))
                libs.append(lib);
        }
    }
    return libs;
}

QHash<QString, QString> bundledInstallScriptMap()
{
    // From public appmanifest_228980.acf InstallScripts (depot → path under Steamworks Shared).
    static const QHash<QString, QString> map = {
        {QStringLiteral("228981"), QStringLiteral("_CommonRedist/vcredist/2005/installscript.vdf")},
        {QStringLiteral("228982"), QStringLiteral("_CommonRedist/vcredist/2008/installscript.vdf")},
        {QStringLiteral("228983"), QStringLiteral("_CommonRedist/vcredist/2010/installscript.vdf")},
        {QStringLiteral("228984"), QStringLiteral("_CommonRedist/vcredist/2012/installscript.vdf")},
        {QStringLiteral("228985"), QStringLiteral("_CommonRedist/vcredist/2013/installscript.vdf")},
        {QStringLiteral("228986"), QStringLiteral("_CommonRedist/vcredist/2015/installscript.vdf")},
        {QStringLiteral("228987"), QStringLiteral("_CommonRedist/vcredist/2017/installscript.vdf")},
        {QStringLiteral("228988"), QStringLiteral("_CommonRedist/vcredist/2019/installscript.vdf")},
        {QStringLiteral("228989"), QStringLiteral("_CommonRedist/vcredist/2019/installscript.vdf")},
        {QStringLiteral("228990"), QStringLiteral("_CommonRedist/DirectX/Jun2010/installscript.vdf")},
        {QStringLiteral("229000"), QStringLiteral("_CommonRedist/DotNet/3.5/installscript.vdf")},
        {QStringLiteral("229002"), QStringLiteral("_CommonRedist/DotNet/4.0/installscript.vdf")},
        {QStringLiteral("229003"),
         QStringLiteral("_CommonRedist/DotNet/4.0 Client Profile/installscript.vdf")},
        {QStringLiteral("229004"), QStringLiteral("_CommonRedist/DotNet/4.5.2/installscript.vdf")},
        {QStringLiteral("229005"), QStringLiteral("_CommonRedist/DotNet/4.6/installscript.vdf")},
        {QStringLiteral("229006"), QStringLiteral("_CommonRedist/DotNet/4.5.2/installscript.vdf")},
        {QStringLiteral("229007"), QStringLiteral("_CommonRedist/DotNet/4.8/installscript.vdf")},
        {QStringLiteral("229012"), QStringLiteral("_CommonRedist/XNA/4.0/installscript.vdf")},
        {QStringLiteral("229020"), QStringLiteral("_CommonRedist/OpenAL/2.0.7.0/installscript.vdf")},
    };
    return map;
}

QHash<QString, QString> parseInstallScriptsFromAcf(const QString& acfText)
{
    QHash<QString, QString> out;
    const QRegularExpression sectionRe(
        R"re("InstallScripts"\s*\{([^}]*)\})re",
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression kvRe(R"re("(\d+)"\s+"([^"]+)")re");

    const auto match = sectionRe.match(acfText);
    if (!match.hasMatch())
        return out;

    auto it = kvRe.globalMatch(match.captured(1));
    while (it.hasNext()) {
        const auto m = it.next();
        QString rel = m.captured(2).trimmed();
        rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
        out.insert(m.captured(1), rel);
    }
    return out;
}

QString findAppManifest228980()
{
    for (const QString& steamRoot : candidateSteamRoots()) {
        for (const QString& lib : libraryRootsForSteam(steamRoot)) {
            const QString acf =
                normalizePath(lib) + QStringLiteral("/steamapps/appmanifest_228980.acf");
            if (QFileInfo::exists(acf))
                return acf;
        }
    }
    return {};
}

QHash<QString, QString> liveInstallScriptsMap()
{
    const QString acfPath = findAppManifest228980();
    if (acfPath.isEmpty())
        return {};
    QFile file(acfPath);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return parseInstallScriptsFromAcf(QString::fromUtf8(file.readAll()));
}

} // namespace

QString findSteamworksSharedInstallDir()
{
    for (const QString& steamRoot : candidateSteamRoots()) {
        for (const QString& lib : libraryRootsForSteam(steamRoot)) {
            const QString shared =
                normalizePath(lib) + QStringLiteral("/steamapps/common/Steamworks Shared");
            if (QDir(shared).exists())
                return shared;
        }
    }
    return {};
}

QString findSteamworksCommonRedistRoot()
{
    const QString shared = findSteamworksSharedInstallDir();
    if (shared.isEmpty())
        return {};
    const QString redist = shared + QStringLiteral("/_CommonRedist");
    return QDir(redist).exists() ? redist : QString{};
}

QString installScriptRelativePathForDepot(const QString& depotId)
{
    const QHash<QString, QString> live = liveInstallScriptsMap();
    if (live.contains(depotId))
        return live.value(depotId);

    const QHash<QString, QString> bundled = bundledInstallScriptMap();
    return bundled.value(depotId);
}

QString labelFromInstallScriptRelativePath(const QString& relativePath)
{
    if (relativePath.isEmpty())
        return {};

    QString path = relativePath;
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    // Expect: _CommonRedist/<family>/<version>/installscript*.vdf
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    int familyIdx = -1;
    for (int i = 0; i < parts.size(); ++i) {
        if (parts.at(i).compare(QStringLiteral("_CommonRedist"), Qt::CaseInsensitive) == 0) {
            familyIdx = i + 1;
            break;
        }
    }
    if (familyIdx < 0 || familyIdx >= parts.size())
        return {};

    const QString family = parts.at(familyIdx);
    const QString version = (familyIdx + 1 < parts.size()) ? parts.at(familyIdx + 1) : QString{};
    // Skip filename-looking segments (installscript.vdf).
    const QString versionClean =
        version.endsWith(QStringLiteral(".vdf"), Qt::CaseInsensitive) ? QString{} : version;

    const QString familyLower = family.toLower();
    if (familyLower == QLatin1String("vcredist")) {
        return versionClean.isEmpty() ? QStringLiteral("Visual C++")
                                      : QStringLiteral("Visual C++ %1").arg(versionClean);
    }
    if (familyLower == QLatin1String("dotnet")) {
        return versionClean.isEmpty() ? QStringLiteral(".NET Framework")
                                      : QStringLiteral(".NET Framework %1").arg(versionClean);
    }
    if (familyLower == QLatin1String("directx")) {
        return versionClean.isEmpty() ? QStringLiteral("DirectX")
                                      : QStringLiteral("DirectX %1").arg(versionClean);
    }
    if (familyLower == QLatin1String("openal")) {
        return versionClean.isEmpty() ? QStringLiteral("OpenAL")
                                      : QStringLiteral("OpenAL %1").arg(versionClean);
    }
    if (familyLower == QLatin1String("xna")) {
        return versionClean.isEmpty() ? QStringLiteral("XNA Framework")
                                      : QStringLiteral("XNA Framework %1").arg(versionClean);
    }
    if (familyLower == QLatin1String("physx")) {
        return versionClean.isEmpty() ? QStringLiteral("PhysX")
                                      : QStringLiteral("PhysX %1").arg(versionClean);
    }
    if (versionClean.isEmpty())
        return family;
    return QStringLiteral("%1 %2").arg(family, versionClean);
}

RedistInstallPlan buildRedistInstallPlan(const QString& depotId)
{
    RedistInstallPlan plan;
    plan.depotId = depotId;
    plan.installDir = findSteamworksSharedInstallDir();
    plan.scriptRelativePath = installScriptRelativePathForDepot(depotId);
    if (plan.installDir.isEmpty() || plan.scriptRelativePath.isEmpty())
        return plan;

    plan.scriptAbsolutePath =
        QDir(plan.installDir).filePath(plan.scriptRelativePath);
    plan.scriptAbsolutePath = QDir::cleanPath(plan.scriptAbsolutePath);
    if (!QFileInfo::exists(plan.scriptAbsolutePath)) {
        // Some ACF entries omit installscript_x64; try sibling names.
        const QString primary = plan.scriptAbsolutePath;
        QString altX64 = primary;
        altX64.replace(QStringLiteral("installscript.vdf"), QStringLiteral("installscript_x64.vdf"));
        QString altX86 = primary;
        altX86.replace(QStringLiteral("installscript_x64.vdf"), QStringLiteral("installscript.vdf"));
        plan.scriptAbsolutePath.clear();
        for (const QString& alt : {primary, altX64, altX86}) {
            if (QFileInfo::exists(alt)) {
                plan.scriptAbsolutePath = alt;
                break;
            }
        }
        if (plan.scriptAbsolutePath.isEmpty())
            return plan;
    }

    QFile file(plan.scriptAbsolutePath);
    if (!file.open(QIODevice::ReadOnly))
        return plan;

    plan.steps = parseInstallScriptRunProcess(QString::fromUtf8(file.readAll()));
    resolveInstallDirPlaceholders(&plan.steps, plan.installDir);

    // Drop steps whose process binary is missing (caller may CDN-fallback).
    QVector<RedistInstallStep> present;
    for (const RedistInstallStep& step : plan.steps) {
        if (QFileInfo::exists(step.processPath))
            present.append(step);
    }
    plan.steps = present;
    return plan;
}

} // namespace arachnel::core

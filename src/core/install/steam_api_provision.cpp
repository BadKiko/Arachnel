#include "steam_api_provision.h"

#include "file_utils.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace arachnel::core {
namespace {

QString dllNameForBits(int bits)
{
    return bits == 64 ? QStringLiteral("steam_api64.dll") : QStringLiteral("steam_api.dll");
}

QString findSteamApiDll(const QString& root, int bits, int maxEntries)
{
    if (root.isEmpty() || !QDir(root).exists())
        return {};

    QString best;
    QDirIterator it(root, {QStringLiteral("steam_api.dll"), QStringLiteral("steam_api64.dll")},
                    QDir::Files, QDirIterator::Subdirectories);
    const QString want = dllNameForBits(bits).toLower();
    int seen = 0;
    while (it.hasNext() && seen < maxEntries) {
        it.next();
        ++seen;
        if (peImageBits(it.filePath()) != bits)
            continue;
        if (it.fileName().toLower() == want)
            return it.filePath();
        if (best.isEmpty())
            best = it.filePath();
    }
    return best;
}

} // namespace

QString ensureSteamApiDllForExecutable(const QString& installPath, const QString& executablePath)
{
    if (installPath.isEmpty() || executablePath.isEmpty())
        return {};

    const QFileInfo exeInfo(executablePath);
    if (!exeInfo.exists())
        return {};
    const QString exeDir = exeInfo.absolutePath();

    const int bits = peImageBits(executablePath);
    if (bits != 32 && bits != 64)
        return {};

    // 64-bit Steamworks.NET often DllImport "steam_api" while shipping steam_api64.dll.
    QStringList targets;
    if (bits == 64)
        targets = {QStringLiteral("steam_api64.dll"), QStringLiteral("steam_api.dll")};
    else
        targets = {QStringLiteral("steam_api.dll")};

    QStringList provided;
    QString lastSource;
    for (const QString& targetName : targets) {
        const QString targetPath = QDir(exeDir).filePath(targetName);
        if (QFileInfo::exists(targetPath))
            continue;

        // Stay inside this install (and its immediate parent library folder). No Steam
        // common crawl - slow and can borrow the wrong game's DLL.
        QString source = findSteamApiDll(installPath, bits, 8000);
        if (source.isEmpty()) {
            const QString parent = QFileInfo(installPath).absolutePath();
            if (!parent.isEmpty() && parent != installPath)
                source = findSteamApiDll(parent, bits, 4000);
        }
        if (source.isEmpty() || QFileInfo(source).absoluteFilePath() == targetPath)
            continue;
        if (!QFile::copy(source, targetPath))
            continue;
        provided.append(targetName);
        lastSource = source;
    }

    if (provided.isEmpty())
        return {};

    return QCoreApplication::translate(
               "Core",
               "steam_api repair: provided %1 next to the game executable (source: %2)")
        .arg(provided.join(QStringLiteral(", ")), lastSource);
}

} // namespace arachnel::core

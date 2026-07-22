#include "storage_library.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace arachnel::core {

QString defaultStorageLibraryPath()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("C:/Games/Arachnel");
#else
    return QDir::homePath() + QStringLiteral("/Games/Arachnel");
#endif
}

QString normalizedStoragePath(const QString& path)
{
    QString normalized = QDir::fromNativeSeparators(path.trimmed());
    while (normalized.endsWith(QLatin1Char('/')))
        normalized.chop(1);
    return normalized;
}

QString autoStorageLibraryLabel(const QString& path)
{
    const QString normalized = normalizedStoragePath(path);
    if (normalized.isEmpty())
        return QCoreApplication::translate("Core", "Library");

#if defined(Q_OS_WIN)
    if (normalized.size() >= 2 && normalized.at(1) == QLatin1Char(':')) {
        const wchar_t drive[] = {normalized.at(0).toUpper().unicode(), L':', L'\\', L'\0'};
        wchar_t volumeName[MAX_PATH + 1] = {};
        wchar_t fileSystem[MAX_PATH + 1] = {};
        DWORD serial = 0;
        DWORD maxLen = 0;
        DWORD flags = 0;

        QString driveType = QCoreApplication::translate("Core", "Disk");
        if (GetVolumeInformationW(drive, volumeName, MAX_PATH, &serial, &maxLen, &flags,
                                  fileSystem, MAX_PATH)) {
            const QString fs = QString::fromWCharArray(fileSystem);
            if (fs.contains(QStringLiteral("SSD"), Qt::CaseInsensitive)
                || fs.contains(QStringLiteral("NVMe"), Qt::CaseInsensitive))
                driveType = QStringLiteral("SSD");
            else
                driveType = QStringLiteral("HDD");
        }

        const QString letter = normalized.left(2).toUpper();
        const QString volume = QString::fromWCharArray(volumeName).trimmed();
        if (!volume.isEmpty())
            return QStringLiteral("%1 (%2)").arg(volume, letter);
        return QStringLiteral("%1 (%2)").arg(driveType, letter);
    }
#endif

    const QString folder = QFileInfo(normalized).fileName();
    return folder.isEmpty() ? normalized : folder;
}

QString gamesDirForLibrary(const StorageLibrary& library, const QString& gameId)
{
    QString dir = normalizedStoragePath(library.path);
    if (gameId.isEmpty())
        return dir;
    return dir + QLatin1Char('/') + gameId;
}

QString downloadsDirForLibrary(const StorageLibrary& library)
{
    return normalizedStoragePath(library.path) + QStringLiteral("/downloads");
}

} // namespace arachnel::core

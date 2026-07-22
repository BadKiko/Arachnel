#include "install_marker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace arachnel::core {

QString installMarkerPath(const QString& installPath)
{
    if (installPath.trimmed().isEmpty())
        return {};
    return QDir(installPath).absoluteFilePath(QString::fromUtf8(kInstallMarkerFileName));
}

bool hasInstallMarker(const QString& installPath)
{
    const QString path = installMarkerPath(installPath);
    return !path.isEmpty() && QFileInfo::exists(path);
}

bool writeInstallMarker(const QString& installPath, const QString& gameId,
                        const QString& sourceId)
{
    if (installPath.trimmed().isEmpty() || !QFileInfo::exists(installPath))
        return false;

    QDir().mkpath(installPath);
    const QString path = installMarkerPath(installPath);
    if (path.isEmpty())
        return false;

    QJsonObject obj;
    obj.insert(QStringLiteral("schemaVersion"), 1);
    if (!gameId.trimmed().isEmpty())
        obj.insert(QStringLiteral("id"), gameId.trimmed());
    if (!sourceId.trimmed().isEmpty())
        obj.insert(QStringLiteral("sourceId"), sourceId.trimmed());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    file.write("\n");
    return file.commit();
}

} // namespace arachnel::core

#include "settings_identity.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#if defined(Q_OS_WIN)
#include <QtCore/QByteArray>
#endif

namespace arachnel {
namespace {

void copySettingsGroup(QSettings& from, QSettings& to, const QString& group)
{
    from.beginGroup(group);
    const QStringList keys = from.childKeys();
    if (keys.isEmpty()) {
        from.endGroup();
        return;
    }

    to.beginGroup(group);
    for (const QString& key : keys) {
        if (!to.contains(key))
            to.setValue(key, from.value(key));
    }
    to.endGroup();
    from.endGroup();
}

bool directoryLooksLikeAppData(const QString& path)
{
    if (path.isEmpty() || !QFileInfo::exists(path))
        return false;
    return QFileInfo::exists(path + QStringLiteral("/library.json"))
        || QFileInfo::exists(path + QStringLiteral("/settings.json"))
        || QFileInfo::exists(path + QStringLiteral("/plugins"));
}

bool pathIsSelfOrDescendant(const QString& path, const QString& ancestor)
{
    const QString cleanPath = QDir::cleanPath(path);
    const QString cleanAncestor = QDir::cleanPath(ancestor);
    if (cleanPath.compare(cleanAncestor, Qt::CaseInsensitive) == 0)
        return true;
    const QString prefix = cleanAncestor + QLatin1Char('/');
    return cleanPath.startsWith(prefix, Qt::CaseInsensitive);
}

bool copyDirectoryRecursive(const QString& source, const QString& destination)
{
    QDir src(source);
    if (!src.exists())
        return false;

    const QString cleanSource = QDir::cleanPath(source);
    const QString cleanDestination = QDir::cleanPath(destination);
    // AppDataLocation is often %APPDATA%/Arachnel/Arachnel while legacy data lives in
    // %APPDATA%/Arachnel — copying without this guard nests destination into itself forever.
    if (pathIsSelfOrDescendant(cleanDestination, cleanSource)) {
        QDir().mkpath(cleanDestination);
        const auto entries =
            src.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
        for (const QFileInfo& entry : entries) {
            const QString abs = QDir::cleanPath(entry.absoluteFilePath());
            if (pathIsSelfOrDescendant(abs, cleanDestination))
                continue;
            const QString target = cleanDestination + QLatin1Char('/') + entry.fileName();
            if (entry.isDir()) {
                if (!copyDirectoryRecursive(abs, target))
                    return false;
            } else if (!QFile::exists(target) && !QFile::copy(abs, target)) {
                return false;
            }
        }
        return true;
    }

    QDir().mkpath(destination);
    const auto entries =
        src.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo& entry : entries) {
        const QString target = destination + QLatin1Char('/') + entry.fileName();
        if (entry.isDir()) {
            if (!copyDirectoryRecursive(entry.absoluteFilePath(), target))
                return false;
        } else {
            if (QFile::exists(target))
                continue;
            if (!QFile::copy(entry.absoluteFilePath(), target))
                return false;
        }
    }
    return true;
}

void migrateAppDataIfNeeded()
{
    const QString target = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (directoryLooksLikeAppData(target))
        return;

#if defined(Q_OS_WIN)
    const QString roaming = QString::fromLocal8Bit(qgetenv("APPDATA"));
#else
    const QString roaming = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#endif
    if (roaming.isEmpty())
        return;

    const QStringList candidates = {
        roaming + QStringLiteral("/Arachnel"),
        roaming + QStringLiteral("/PetWork/Arachnel"),
    };

    for (const QString& candidate : candidates) {
        if (QDir::cleanPath(candidate).compare(QDir::cleanPath(target), Qt::CaseInsensitive) == 0)
            continue;
        if (!directoryLooksLikeAppData(candidate))
            continue;
        QDir().mkpath(target);
        if (copyDirectoryRecursive(candidate, target))
            return;
    }
}

void migrateLegacyQSettings()
{
    QSettings current;
    const QList<QPair<QString, QString>> legacyOrgs = {
        {QStringLiteral("PetWork"), QStringLiteral("Arachnel")},
        {QStringLiteral("Unknown Organization"), QStringLiteral("Arachnel")},
    };

    for (const auto& orgApp : legacyOrgs) {
        QSettings legacy(QSettings::NativeFormat, QSettings::UserScope, orgApp.first, orgApp.second);
        copySettingsGroup(legacy, current, QStringLiteral("appearance"));
        copySettingsGroup(legacy, current, QStringLiteral("catalog"));
    }
    current.sync();
}

} // namespace

void configureApplicationIdentity()
{
    QCoreApplication::setOrganizationName(QStringLiteral("Arachnel"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("arachnel.app"));
    QCoreApplication::setApplicationName(QStringLiteral("Arachnel"));
#ifndef ARACHNEL_VERSION
#define ARACHNEL_VERSION "dev"
#endif
    QCoreApplication::setApplicationVersion(QStringLiteral(ARACHNEL_VERSION));

    migrateAppDataIfNeeded();
    migrateLegacyQSettings();
}

} // namespace arachnel

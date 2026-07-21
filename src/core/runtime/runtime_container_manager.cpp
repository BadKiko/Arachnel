#include "runtime_container_manager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace arachnel::core {

namespace {

QString containersRoot()
{
    // Same layout as ProtonManager::compatDataRoot — one prefix per game.
    const QString root =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/compatdata");
    QDir().mkpath(root);
    return root;
}

QString safeGameId(const QString& gameId)
{
    QString safe = gameId.trimmed();
    if (safe.isEmpty())
        safe = QStringLiteral("default");
    safe.replace(QLatin1Char('/'), QLatin1Char('_'));
    safe.replace(QLatin1Char('\\'), QLatin1Char('_'));
    return safe;
}

QJsonObject readStateObject(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

void writeStateObject(const QString& path, const QJsonObject& obj)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

} // namespace

QString RuntimeContainerManager::containerRootForGame(const QString& gameId) const
{
    const QString path = containersRoot() + QLatin1Char('/') + safeGameId(gameId);
    QDir().mkpath(path);
    return path;
}

QString RuntimeContainerManager::cacheDirForGame(const QString& gameId) const
{
    const QString path = containerRootForGame(gameId) + QStringLiteral("/cache");
    QDir().mkpath(path);
    return path;
}

QString RuntimeContainerManager::stateFileForGame(const QString& gameId) const
{
    return containerRootForGame(gameId) + QStringLiteral("/state.json");
}

QString RuntimeContainerManager::prefixDirForGame(const QString& gameId) const
{
    const QString path = containerRootForGame(gameId) + QStringLiteral("/pfx");
    QDir().mkpath(path);
    return path;
}

QStringList RuntimeContainerManager::installedDepotIds(const QString& gameId) const
{
    const QJsonObject root = readStateObject(stateFileForGame(gameId));
    QStringList ids;
    for (const QJsonValue& value : root.value(QStringLiteral("installedDepots")).toArray())
        ids.append(value.toString());
    return ids;
}

void RuntimeContainerManager::markDepotInstalled(const QString& gameId, const QString& steamAppId,
                                                 const QString& depotId)
{
    QJsonObject root = readStateObject(stateFileForGame(gameId));
    if (!steamAppId.isEmpty())
        root.insert(QStringLiteral("steamAppId"), steamAppId);

    QJsonArray depots = root.value(QStringLiteral("installedDepots")).toArray();
    for (const QJsonValue& value : depots) {
        if (value.toString() == depotId)
            return;
    }
    depots.append(depotId);
    root.insert(QStringLiteral("installedDepots"), depots);
    writeStateObject(stateFileForGame(gameId), root);
}

void RuntimeContainerManager::unmarkDepotInstalled(const QString& gameId, const QString& depotId)
{
    QJsonObject root = readStateObject(stateFileForGame(gameId));
    QJsonArray depots = root.value(QStringLiteral("installedDepots")).toArray();
    QJsonArray kept;
    for (const QJsonValue& value : depots) {
        if (value.toString() != depotId)
            kept.append(value);
    }
    if (kept.size() == depots.size())
        return;
    root.insert(QStringLiteral("installedDepots"), kept);
    writeStateObject(stateFileForGame(gameId), root);
}

bool RuntimeContainerManager::isDepotInstalledForGame(const QString& gameId,
                                                      const QString& depotId) const
{
    return installedDepotIds(gameId).contains(depotId);
}

} // namespace arachnel::core

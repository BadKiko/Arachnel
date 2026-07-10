#include "library_store.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <algorithm>

namespace arachnel::core {

namespace {

QString libraryFilePath()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/library.json");
}

QJsonArray componentsToJson(const QVector<InstalledComponent>& components)
{
    QJsonArray array;
    for (const auto& component : components) {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), component.id);
        obj.insert(QStringLiteral("title"), component.title);
        obj.insert(QStringLiteral("uploadDate"), component.uploadDate);
        obj.insert(QStringLiteral("installed"), component.installed);
        array.append(obj);
    }
    return array;
}

QVector<InstalledComponent> componentsFromJson(const QJsonArray& array)
{
    QVector<InstalledComponent> components;
    components.reserve(array.size());
    for (const QJsonValue& value : array) {
        const QJsonObject obj = value.toObject();
        InstalledComponent component;
        component.id = obj.value(QStringLiteral("id")).toString();
        component.title = obj.value(QStringLiteral("title")).toString();
        component.uploadDate = obj.value(QStringLiteral("uploadDate")).toString();
        component.installed = obj.value(QStringLiteral("installed")).toBool();
        components.append(component);
    }
    return components;
}

} // namespace

LibraryStore::LibraryStore(QObject* parent)
    : QObject(parent)
{
}

void LibraryStore::setGames(QVector<LibraryGame> games)
{
    m_games = std::move(games);
    emit gamesChanged();
    save();
}

const LibraryGame* LibraryStore::gameById(const QString& id) const
{
    for (const auto& game : m_games) {
        if (game.id == id)
            return &game;
    }
    return nullptr;
}

void LibraryStore::upsertGame(const LibraryGame& game)
{
    for (auto& existing : m_games) {
        if (existing.id == game.id) {
            existing = game;
            emit gamesChanged();
            save();
            return;
        }
    }
    m_games.append(game);
    emit gamesChanged();
    save();
}

void LibraryStore::removeGame(const QString& id)
{
    const auto it = std::find_if(m_games.begin(), m_games.end(),
                                 [&](const LibraryGame& game) { return game.id == id; });
    if (it == m_games.end())
        return;
    m_games.erase(it);
    emit gamesChanged();
    save();
}

void LibraryStore::load()
{
    QFile file(libraryFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonArray array = QJsonDocument::fromJson(file.readAll()).array();
    QVector<LibraryGame> games;
    games.reserve(array.size());
    for (const QJsonValue& value : array) {
        const QJsonObject obj = value.toObject();
        LibraryGame game;
        game.id = obj.value(QStringLiteral("id")).toString();
        game.title = obj.value(QStringLiteral("title")).toString();
        game.coverUrl = obj.value(QStringLiteral("coverUrl")).toString();
        game.sourceId = obj.value(QStringLiteral("sourceId")).toString();
        game.sourceName = obj.value(QStringLiteral("sourceName")).toString();
        game.version = obj.value(QStringLiteral("version")).toString();
        game.installPath = obj.value(QStringLiteral("installPath")).toString();
        game.description = obj.value(QStringLiteral("description")).toString();
        game.genres = obj.value(QStringLiteral("genres")).toString();
        game.sizeLabel = obj.value(QStringLiteral("sizeLabel")).toString();
        game.installKind =
            static_cast<InstallKind>(obj.value(QStringLiteral("installKind")).toInt());
        game.hasUpdate = obj.value(QStringLiteral("hasUpdate")).toBool();
        game.uploadDate = obj.value(QStringLiteral("uploadDate")).toString();
        game.magnetUri = obj.value(QStringLiteral("magnetUri")).toString();
        game.downloadPath = obj.value(QStringLiteral("downloadPath")).toString();
        game.components = componentsFromJson(obj.value(QStringLiteral("components")).toArray());
        games.append(game);
    }
    m_games = std::move(games);
    emit gamesChanged();
}

void LibraryStore::save()
{
    QJsonArray array;
    for (const auto& game : m_games) {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), game.id);
        obj.insert(QStringLiteral("title"), game.title);
        obj.insert(QStringLiteral("coverUrl"), game.coverUrl);
        obj.insert(QStringLiteral("sourceId"), game.sourceId);
        obj.insert(QStringLiteral("sourceName"), game.sourceName);
        obj.insert(QStringLiteral("version"), game.version);
        obj.insert(QStringLiteral("installPath"), game.installPath);
        obj.insert(QStringLiteral("description"), game.description);
        obj.insert(QStringLiteral("genres"), game.genres);
        obj.insert(QStringLiteral("sizeLabel"), game.sizeLabel);
        obj.insert(QStringLiteral("installKind"), static_cast<int>(game.installKind));
        obj.insert(QStringLiteral("hasUpdate"), game.hasUpdate);
        obj.insert(QStringLiteral("uploadDate"), game.uploadDate);
        obj.insert(QStringLiteral("magnetUri"), game.magnetUri);
        obj.insert(QStringLiteral("downloadPath"), game.downloadPath);
        obj.insert(QStringLiteral("components"), componentsToJson(game.components));
        array.append(obj);
    }

    QFile file(libraryFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
}

} // namespace arachnel::core

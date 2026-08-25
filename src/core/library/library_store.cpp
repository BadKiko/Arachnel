#include "library_store.h"

#include "catalog_types.h"

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
        obj.insert(QStringLiteral("enabled"), component.enabled);
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
        // Missing key → on (legacy library.json).
        component.enabled = obj.contains(QStringLiteral("enabled"))
                                ? obj.value(QStringLiteral("enabled")).toBool()
                                : true;
        components.append(component);
    }
    return components;
}

QJsonArray launchOptionsToJson(const QVector<GameLaunchOption>& options)
{
    QJsonArray array;
    for (const auto& opt : options) {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), opt.id);
        obj.insert(QStringLiteral("title"), opt.title);
        obj.insert(QStringLiteral("executable"), opt.executable);
        obj.insert(QStringLiteral("workingDirectory"), opt.workingDirectory);
        obj.insert(QStringLiteral("arguments"), QJsonArray::fromStringList(opt.arguments));
        obj.insert(QStringLiteral("type"), opt.type);
        obj.insert(QStringLiteral("isDefault"), opt.isDefault);
        array.append(obj);
    }
    return array;
}

QVector<GameLaunchOption> launchOptionsFromJson(const QJsonArray& array)
{
    QVector<GameLaunchOption> options;
    options.reserve(array.size());
    for (const QJsonValue& value : array) {
        const QJsonObject obj = value.toObject();
        GameLaunchOption opt;
        opt.id = obj.value(QStringLiteral("id")).toString();
        opt.title = obj.value(QStringLiteral("title")).toString();
        opt.executable = obj.value(QStringLiteral("executable")).toString();
        opt.workingDirectory = obj.value(QStringLiteral("workingDirectory")).toString();
        const QJsonArray argsArr = obj.value(QStringLiteral("arguments")).toArray();
        for (const QJsonValue& a : argsArr)
            opt.arguments.append(a.toString());
        opt.type = obj.value(QStringLiteral("type")).toString();
        opt.isDefault = obj.value(QStringLiteral("isDefault")).toBool();
        options.append(opt);
    }
    return options;
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
    const QString resolved = repairCatalogEntryId(id);
    for (const auto& game : m_games) {
        if (game.id == resolved || game.id == id)
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
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray())
        return;

    QVector<LibraryGame> games;
    const QJsonArray array = doc.array();
    games.reserve(array.size());
    for (const auto& value : array) {
        if (!value.isObject())
            continue;
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
        game.autoUpdate = obj.value(QStringLiteral("autoUpdate")).toBool(true);
        game.uploadDate = obj.value(QStringLiteral("uploadDate")).toString();
        game.magnetUri = obj.value(QStringLiteral("magnetUri")).toString();
        game.downloadPath = obj.value(QStringLiteral("downloadPath")).toString();
        game.libraryId = obj.value(QStringLiteral("libraryId")).toString();
        game.lastPlayedAt = obj.value(QStringLiteral("lastPlayedAt")).toString();
        game.launchArgs = obj.value(QStringLiteral("launchArgs")).toString();
        game.executableOverride = obj.value(QStringLiteral("executableOverride")).toString();
        game.protonId = obj.value(QStringLiteral("protonId")).toString();
        game.steamAppId = obj.value(QStringLiteral("steamAppId")).toString();
        game.selectedLaunchOptionId = obj.value(QStringLiteral("selectedLaunchOptionId")).toString();
        game.launchOptions = launchOptionsFromJson(obj.value(QStringLiteral("launchOptions")).toArray());
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
        obj.insert(QStringLiteral("autoUpdate"), game.autoUpdate);
        obj.insert(QStringLiteral("uploadDate"), game.uploadDate);
        obj.insert(QStringLiteral("magnetUri"), game.magnetUri);
        obj.insert(QStringLiteral("downloadPath"), game.downloadPath);
        obj.insert(QStringLiteral("libraryId"), game.libraryId);
        obj.insert(QStringLiteral("lastPlayedAt"), game.lastPlayedAt);
        obj.insert(QStringLiteral("launchArgs"), game.launchArgs);
        obj.insert(QStringLiteral("executableOverride"), game.executableOverride);
        obj.insert(QStringLiteral("protonId"), game.protonId);
        obj.insert(QStringLiteral("steamAppId"), game.steamAppId);
        if (!game.selectedLaunchOptionId.isEmpty())
            obj.insert(QStringLiteral("selectedLaunchOptionId"), game.selectedLaunchOptionId);
        if (!game.launchOptions.isEmpty())
            obj.insert(QStringLiteral("launchOptions"), launchOptionsToJson(game.launchOptions));
        obj.insert(QStringLiteral("components"), componentsToJson(game.components));
        array.append(obj);
    }

    QFile file(libraryFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
}

} // namespace arachnel::core

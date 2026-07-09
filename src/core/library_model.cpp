#include "library_model.h"

namespace arachnel::core {

LibraryModel::LibraryModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int LibraryModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_games.size();
}

QVariant LibraryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_games.size())
        return {};

    const auto& game = m_games.at(index.row());
    switch (role) {
    case GameIdRole:
        return game.id;
    case TitleRole:
        return game.title;
    case CoverUrlRole:
        return game.coverUrl;
    case SourceIdRole:
        return game.sourceId;
    case SourceNameRole:
        return game.sourceName;
    case VersionRole:
        return game.version;
    case InstallPathRole:
        return game.installPath;
    case DescriptionRole:
        return game.description;
    case GenresRole:
        return game.genres;
    case SizeLabelRole:
        return game.sizeLabel;
    case InstallKindRole:
        return static_cast<int>(game.installKind);
    case InstallKindLabelRole:
        return installKindLabel(game.installKind);
    case HasUpdateRole:
        return game.hasUpdate;
    default:
        return {};
    }
}

QHash<int, QByteArray> LibraryModel::roleNames() const
{
    return {
        {GameIdRole, "gameId"},
        {TitleRole, "title"},
        {CoverUrlRole, "coverUrl"},
        {SourceIdRole, "sourceId"},
        {SourceNameRole, "sourceName"},
        {VersionRole, "version"},
        {InstallPathRole, "installPath"},
        {DescriptionRole, "description"},
        {GenresRole, "genres"},
        {SizeLabelRole, "sizeLabel"},
        {InstallKindRole, "installKind"},
        {InstallKindLabelRole, "installKindLabel"},
        {HasUpdateRole, "hasUpdate"},
    };
}

void LibraryModel::setGames(QVector<LibraryGame> games)
{
    beginResetModel();
    m_games = std::move(games);
    endResetModel();
}

const LibraryGame* LibraryModel::gameById(const QString& id) const
{
    for (const auto& game : m_games) {
        if (game.id == id)
            return &game;
    }
    return nullptr;
}

QVariantMap LibraryModel::toMap(const LibraryGame& game) const
{
    return {
        {QStringLiteral("gameId"), game.id},
        {QStringLiteral("entryId"), game.id},
        {QStringLiteral("title"), game.title},
        {QStringLiteral("coverUrl"), game.coverUrl},
        {QStringLiteral("sourceId"), game.sourceId},
        {QStringLiteral("sourceName"), game.sourceName},
        {QStringLiteral("version"), game.version},
        {QStringLiteral("installPath"), game.installPath},
        {QStringLiteral("description"), game.description},
        {QStringLiteral("genres"), game.genres},
        {QStringLiteral("sizeLabel"), game.sizeLabel},
        {QStringLiteral("installKind"), static_cast<int>(game.installKind)},
        {QStringLiteral("installKindLabel"), installKindLabel(game.installKind)},
        {QStringLiteral("hasUpdate"), game.hasUpdate},
        {QStringLiteral("installed"), true},
    };
}

QVariantMap LibraryModel::gameAt(int row) const
{
    if (row < 0 || row >= m_games.size())
        return {};
    return toMap(m_games.at(row));
}

QVariantMap LibraryModel::gameInfo(const QString& id) const
{
    const LibraryGame* game = gameById(id);
    if (!game)
        return {};
    return toMap(*game);
}

int LibraryModel::updateCount() const
{
    int count = 0;
    for (const auto& game : m_games) {
        if (game.hasUpdate)
            ++count;
    }
    return count;
}

} // namespace arachnel::core

#include "friends_model.h"

#include <algorithm>

namespace arachnel::core {

FriendsModel::FriendsModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int FriendsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_friends.size();
}

QVariant FriendsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_friends.size())
        return {};
    const FriendEntry& entry = m_friends.at(index.row());
    switch (role) {
    case FriendIdRole:
        return entry.friendId;
    case NicknameRole:
        return entry.nickname;
    case PublicKeyRole:
        return entry.publicKey;
    case OnlineRole:
        return entry.online;
    case CurrentGameIdRole:
        return entry.currentGameId;
    case CurrentGameTitleRole:
        return entry.currentGameTitle;
    case CurrentGameCoverUrlRole:
        return entry.currentGameCoverUrl;
    case AddedAtRole:
        return entry.addedAt;
    case LastSeenAtRole:
        return entry.lastSeenAt;
    case SuggestedGameIdRole:
        return entry.suggestedGameId;
    case SuggestedGameTitleRole:
        return entry.suggestedGameTitle;
    case SuggestedCoverUrlRole:
        return entry.suggestedCoverUrl;
    case SuggestedAtRole:
        return entry.suggestedAt;
    default:
        return {};
    }
}

QHash<int, QByteArray> FriendsModel::roleNames() const
{
    return {
        {FriendIdRole, "friendId"},
        {NicknameRole, "nickname"},
        {PublicKeyRole, "publicKey"},
        {OnlineRole, "online"},
        {CurrentGameIdRole, "currentGameId"},
        {CurrentGameTitleRole, "currentGameTitle"},
        {CurrentGameCoverUrlRole, "currentGameCoverUrl"},
        {AddedAtRole, "addedAt"},
        {LastSeenAtRole, "lastSeenAt"},
        {SuggestedGameIdRole, "suggestedGameId"},
        {SuggestedGameTitleRole, "suggestedGameTitle"},
        {SuggestedCoverUrlRole, "suggestedCoverUrl"},
        {SuggestedAtRole, "suggestedAt"},
    };
}

void FriendsModel::setFriends(QVector<FriendEntry> friends)
{
    std::sort(friends.begin(), friends.end(), [](const FriendEntry& a, const FriendEntry& b) {
        if (a.online != b.online)
            return a.online && !b.online;
        return QString::localeAwareCompare(a.nickname, b.nickname) < 0;
    });
    beginResetModel();
    m_friends = std::move(friends);
    endResetModel();
    emit countChanged();
}

QVariantMap FriendsModel::friendInfo(int row) const
{
    if (row < 0 || row >= m_friends.size())
        return {};
    const FriendEntry& entry = m_friends.at(row);
    return {
        {QStringLiteral("friendId"), entry.friendId},
        {QStringLiteral("nickname"), entry.nickname},
        {QStringLiteral("publicKey"), entry.publicKey},
        {QStringLiteral("online"), entry.online},
        {QStringLiteral("currentGameId"), entry.currentGameId},
        {QStringLiteral("currentGameTitle"), entry.currentGameTitle},
        {QStringLiteral("currentGameCoverUrl"), entry.currentGameCoverUrl},
        {QStringLiteral("addedAt"), entry.addedAt},
        {QStringLiteral("lastSeenAt"), entry.lastSeenAt},
        {QStringLiteral("suggestedGameId"), entry.suggestedGameId},
        {QStringLiteral("suggestedGameTitle"), entry.suggestedGameTitle},
        {QStringLiteral("suggestedCoverUrl"), entry.suggestedCoverUrl},
        {QStringLiteral("suggestedAt"), entry.suggestedAt},
    };
}

int FriendsModel::indexOfFriend(const QString& friendId) const
{
    for (int i = 0; i < m_friends.size(); ++i) {
        if (m_friends.at(i).friendId == friendId)
            return i;
    }
    return -1;
}

} // namespace arachnel::core

#pragma once

#include "social_types.h"

#include <QAbstractListModel>
#include <QVariantMap>

namespace arachnel::core {

class FriendsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        FriendIdRole = Qt::UserRole + 1,
        NicknameRole,
        PublicKeyRole,
        OnlineRole,
        CurrentGameIdRole,
        CurrentGameTitleRole,
        CurrentGameCoverUrlRole,
        AddedAtRole,
        LastSeenAtRole,
        SuggestedGameIdRole,
        SuggestedGameTitleRole,
        SuggestedCoverUrlRole,
        SuggestedAtRole,
    };
    Q_ENUM(Role)

    explicit FriendsModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_friends.size(); }
    const QVector<FriendEntry>& friends() const { return m_friends; }
    void setFriends(QVector<FriendEntry> friends);
    Q_INVOKABLE QVariantMap friendInfo(int row) const;
    Q_INVOKABLE int indexOfFriend(const QString& friendId) const;

signals:
    void countChanged();

private:
    QVector<FriendEntry> m_friends;
};

} // namespace arachnel::core

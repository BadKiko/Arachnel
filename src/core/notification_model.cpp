#include "notification_model.h"

#include <QDateTime>
#include <QUuid>

namespace arachnel::core {

QString notificationKindForMessage(const QString& message)
{
    const QString lower = message.toLower();
    if (lower.contains(QStringLiteral("ошибка")) || lower.contains(QStringLiteral("не удалось"))
        || lower.contains(QStringLiteral("не найден")) || lower.contains(QStringLiteral("error"))
        || lower.contains(QStringLiteral("failed")) || lower.contains(QStringLiteral("not found")))
        return QStringLiteral("error");
    if (lower.contains(QStringLiteral("установлен")) || lower.contains(QStringLiteral("удалена"))
        || lower.contains(QStringLiteral("перенесена"))
        || lower.contains(QStringLiteral("доступно обновлений"))
        || lower.contains(QStringLiteral("installed")) || lower.contains(QStringLiteral("removed"))
        || lower.contains(QStringLiteral("moved")) || lower.contains(QStringLiteral("update(s) available")))
        return QStringLiteral("success");
    return QStringLiteral("info");
}

NotificationModel::NotificationModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int NotificationModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant NotificationModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const auto& entry = m_entries.at(index.row());
    switch (role) {
    case NotificationIdRole:
        return entry.id;
    case MessageRole:
        return entry.message;
    case KindRole:
        return entry.kind;
    case CreatedAtRole:
        return entry.createdAt;
    case ReadRole:
        return entry.read;
    default:
        return {};
    }
}

QHash<int, QByteArray> NotificationModel::roleNames() const
{
    return {
        {NotificationIdRole, "notificationId"},
        {MessageRole, "message"},
        {KindRole, "kind"},
        {CreatedAtRole, "createdAt"},
        {ReadRole, "read"},
    };
}

int NotificationModel::unreadCount() const
{
    int count = 0;
    for (const auto& entry : m_entries) {
        if (!entry.read)
            ++count;
    }
    return count;
}

void NotificationModel::add(const QString& message, const QString& kind)
{
    if (message.isEmpty())
        return;

    NotificationEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.message = message;
    entry.kind = kind.isEmpty() ? notificationKindForMessage(message) : kind;
    entry.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    entry.read = false;

    if (m_entries.size() >= kMaxEntries) {
        beginRemoveRows({}, kMaxEntries - 1, kMaxEntries - 1);
        m_entries.removeLast();
        endRemoveRows();
    }

    beginInsertRows({}, 0, 0);
    m_entries.prepend(entry);
    endInsertRows();

    emit countChanged();
    emit unreadCountChanged();
}

void NotificationModel::markAllRead()
{
    if (unreadCount() == 0)
        return;

    for (auto& entry : m_entries)
        entry.read = true;

    emit dataChanged(index(0), index(m_entries.size() - 1), {ReadRole});
    emit unreadCountChanged();
}

void NotificationModel::clear()
{
    if (m_entries.isEmpty())
        return;

    beginResetModel();
    m_entries.clear();
    endResetModel();

    emit countChanged();
    emit unreadCountChanged();
}

} // namespace arachnel::core

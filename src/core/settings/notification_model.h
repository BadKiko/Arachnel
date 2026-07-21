#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

namespace arachnel::core {

struct NotificationEntry {
    QString id;
    QString message;
    QString kind;
    QString createdAt;
    bool read = false;
};

class NotificationModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)

public:
    enum Role {
        NotificationIdRole = Qt::UserRole + 1,
        MessageRole,
        KindRole,
        CreatedAtRole,
        ReadRole,
    };
    Q_ENUM(Role)

    explicit NotificationModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_entries.size(); }
    int unreadCount() const;

    void add(const QString& message, const QString& kind);
    void markAllRead();
    void clear();

signals:
    void countChanged();
    void unreadCountChanged();

private:
    static constexpr int kMaxEntries = 64;

    QVector<NotificationEntry> m_entries;
};

QString notificationKindForMessage(const QString& message);

} // namespace arachnel::core

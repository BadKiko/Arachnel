#pragma once

#include "storage_library.h"

#include <QAbstractListModel>
#include <QVector>

namespace arachnel::core {

class SettingsStore;

class StorageLibraryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY librariesChanged)
    Q_PROPERTY(QString defaultLibraryId READ defaultLibraryId NOTIFY librariesChanged)

public:
    enum Role {
        LibraryIdRole = Qt::UserRole + 1,
        LabelRole,
        PathRole,
        IsDefaultRole,
    };
    Q_ENUM(Role)

    explicit StorageLibraryModel(SettingsStore* store, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    QString defaultLibraryId() const;

    const QVector<StorageLibrary>& libraries() const { return m_libraries; }
    void setLibraries(QVector<StorageLibrary> libraries);

    Q_INVOKABLE QString addLibrary(const QString& path, const QString& label = {});
    Q_INVOKABLE bool removeLibrary(const QString& id);
    Q_INVOKABLE bool setDefaultLibrary(const QString& id);
    Q_INVOKABLE bool updateLibraryPath(const QString& id, const QString& path);
    Q_INVOKABLE QString libraryPath(const QString& id) const;
    Q_INVOKABLE QString downloadsPath(const QString& id) const;
    Q_INVOKABLE QString gameDir(const QString& libraryId, const QString& gameId) const;
    Q_INVOKABLE QVariantMap libraryInfo(const QString& id) const;
    Q_INVOKABLE int indexOfLibrary(const QString& id) const;

    const StorageLibrary* libraryById(const QString& id) const;

signals:
    void librariesChanged();

private:
    void ensureDefault();
    void syncLegacyRoots();

    SettingsStore* m_store = nullptr;
    QVector<StorageLibrary> m_libraries;
};

} // namespace arachnel::core

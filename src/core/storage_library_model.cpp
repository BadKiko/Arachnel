#include "storage_library_model.h"

#include "settings_store.h"

#include <QUuid>

namespace arachnel::core {

StorageLibraryModel::StorageLibraryModel(SettingsStore* store, QObject* parent)
    : QAbstractListModel(parent)
    , m_store(store)
{
}

int StorageLibraryModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_libraries.size();
}

QVariant StorageLibraryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_libraries.size())
        return {};

    const auto& library = m_libraries.at(index.row());
    switch (role) {
    case LibraryIdRole:
        return library.id;
    case LabelRole:
        return library.label;
    case PathRole:
        return library.path;
    case IsDefaultRole:
        return library.isDefault;
    default:
        return {};
    }
}

QHash<int, QByteArray> StorageLibraryModel::roleNames() const
{
    return {
        {LibraryIdRole, "libraryId"},
        {LabelRole, "label"},
        {PathRole, "path"},
        {IsDefaultRole, "isDefault"},
    };
}

int StorageLibraryModel::count() const
{
    return m_libraries.size();
}

QString StorageLibraryModel::defaultLibraryId() const
{
    for (const auto& library : m_libraries) {
        if (library.isDefault)
            return library.id;
    }
    return m_libraries.isEmpty() ? QString() : m_libraries.first().id;
}

void StorageLibraryModel::setLibraries(QVector<StorageLibrary> libraries)
{
    beginResetModel();
    m_libraries = std::move(libraries);
    ensureDefault();
    endResetModel();
    syncLegacyRoots();
    emit librariesChanged();
}

QString StorageLibraryModel::addLibrary(const QString& path, const QString& label)
{
    const QString normalized = normalizedStoragePath(path);
    if (normalized.isEmpty())
        return {};

    for (const auto& existing : m_libraries) {
        if (existing.path.compare(normalized, Qt::CaseInsensitive) == 0)
            return existing.id;
    }

    StorageLibrary library;
    library.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    library.path = normalized;
    library.label = label.trimmed().isEmpty() ? autoStorageLibraryLabel(normalized) : label.trimmed();
    library.isDefault = m_libraries.isEmpty();

    const int row = m_libraries.size();
    beginInsertRows({}, row, row);
    m_libraries.append(library);
    endInsertRows();

    ensureDefault();
    syncLegacyRoots();
    if (m_store)
        m_store->save();
    emit librariesChanged();
    return library.id;
}

bool StorageLibraryModel::removeLibrary(const QString& id)
{
    if (m_libraries.size() <= 1)
        return false;

    int row = -1;
    for (int i = 0; i < m_libraries.size(); ++i) {
        if (m_libraries.at(i).id == id) {
            row = i;
            break;
        }
    }
    if (row < 0)
        return false;

    const bool wasDefault = m_libraries.at(row).isDefault;
    beginRemoveRows({}, row, row);
    m_libraries.removeAt(row);
    endRemoveRows();

    if (wasDefault && !m_libraries.isEmpty())
        m_libraries[0].isDefault = true;

    ensureDefault();
    syncLegacyRoots();
    if (m_store)
        m_store->save();
    emit librariesChanged();
    return true;
}

bool StorageLibraryModel::setDefaultLibrary(const QString& id)
{
    bool changed = false;
    for (auto& library : m_libraries) {
        const bool next = library.id == id;
        if (library.isDefault != next) {
            library.isDefault = next;
            changed = true;
        }
    }
    if (!changed)
        return false;

    emit dataChanged(index(0), index(qMax(0, rowCount() - 1)), {IsDefaultRole});
    syncLegacyRoots();
    if (m_store)
        m_store->save();
    emit librariesChanged();
    return true;
}

bool StorageLibraryModel::updateLibraryPath(const QString& id, const QString& path)
{
    const QString normalized = normalizedStoragePath(path);
    if (normalized.isEmpty())
        return false;

    for (int i = 0; i < m_libraries.size(); ++i) {
        if (m_libraries.at(i).id != id)
            continue;
        m_libraries[i].path = normalized;
        if (m_libraries[i].label.isEmpty())
            m_libraries[i].label = autoStorageLibraryLabel(normalized);
        const QModelIndex idx = index(i);
        emit dataChanged(idx, idx, {PathRole, LabelRole});
        syncLegacyRoots();
        if (m_store)
            m_store->save();
        emit librariesChanged();
        return true;
    }
    return false;
}

QString StorageLibraryModel::libraryPath(const QString& id) const
{
    const StorageLibrary* library = libraryById(id);
    return library ? library->path : QString();
}

QString StorageLibraryModel::downloadsPath(const QString& id) const
{
    const StorageLibrary* library = libraryById(id);
    return library ? downloadsDirForLibrary(*library) : QString();
}

QString StorageLibraryModel::gameDir(const QString& libraryId, const QString& gameId) const
{
    const StorageLibrary* library = libraryById(libraryId);
    if (!library)
        return {};
    return gamesDirForLibrary(*library, gameId);
}

QVariantMap StorageLibraryModel::libraryInfo(const QString& id) const
{
    const StorageLibrary* library = libraryById(id);
    if (!library)
        return {};

    return {
        {QStringLiteral("libraryId"), library->id},
        {QStringLiteral("label"), library->label},
        {QStringLiteral("path"), library->path},
        {QStringLiteral("isDefault"), library->isDefault},
        {QStringLiteral("downloadsPath"), downloadsDirForLibrary(*library)},
    };
}

int StorageLibraryModel::indexOfLibrary(const QString& id) const
{
    for (int i = 0; i < m_libraries.size(); ++i) {
        if (m_libraries.at(i).id == id)
            return i;
    }
    return -1;
}

const StorageLibrary* StorageLibraryModel::libraryById(const QString& id) const
{
    for (const auto& library : m_libraries) {
        if (library.id == id)
            return &library;
    }
    return nullptr;
}

void StorageLibraryModel::ensureDefault()
{
    if (m_libraries.isEmpty())
        return;

    bool hasDefault = false;
    for (const auto& library : m_libraries) {
        if (library.isDefault) {
            hasDefault = true;
            break;
        }
    }
    if (!hasDefault)
        m_libraries[0].isDefault = true;
}

void StorageLibraryModel::syncLegacyRoots()
{
    if (!m_store)
        return;

    const StorageLibrary* def = libraryById(defaultLibraryId());
    if (!def)
        return;

    m_store->syncLegacyRootsFromLibrary(*def);
}

} // namespace arachnel::core

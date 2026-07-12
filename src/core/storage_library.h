#pragma once

#include <QString>

namespace arachnel::core {

struct StorageLibrary {
    QString id;
    QString label;
    QString path;
    bool isDefault = false;
};

QString defaultStorageLibraryPath();
QString autoStorageLibraryLabel(const QString& path);
QString normalizedStoragePath(const QString& path);
QString gamesDirForLibrary(const StorageLibrary& library, const QString& gameId = {});
QString downloadsDirForLibrary(const StorageLibrary& library);

} // namespace arachnel::core

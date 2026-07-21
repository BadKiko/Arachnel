#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace arachnel::core {

struct RuntimeDepotRef {
    QString depotId;
    QString label;
    QString osList; // windows | linux | macos | empty = any
};

struct RuntimeEnsureResult {
    bool success = false;
    QString error;
    QStringList installedLabels;
    QStringList skippedLabels;
};

struct RuntimeEnsureRequest {
    QString gameId;
    QString steamAppId;
    QString title;
    QString installPath;
};

} // namespace arachnel::core

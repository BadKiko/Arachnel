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

/** One Steam installscript "Run Process" step (after %INSTALLDIR% resolve). */
struct RedistInstallStep {
    QString processPath;
    QStringList arguments;
    QString hasRunKey;
    bool require64BitWindows = false;
};

/** Plan to install a Steamworks Shared depot via InstallScripts / VDF. */
struct RedistInstallPlan {
    QString depotId;
    QString scriptRelativePath;
    QString installDir; // Steamworks Shared root
    QString scriptAbsolutePath;
    QVector<RedistInstallStep> steps;
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
    /** Per-game Proton id (empty = settings default). Used when installing redists. */
    QString protonId;
};

} // namespace arachnel::core

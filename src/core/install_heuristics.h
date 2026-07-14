#pragma once

#include "install_analysis.h"

#include <QString>
#include <QStringList>

namespace arachnel::core {

bool isExcludedGameExecutable(const QString& fileName);
QString findGameExecutableInTree(const QString& rootDir);
QString findSetupExecutableInTree(const QString& rootDir);
bool isInnoSetupExecutable(const QString& setupPath);
QString findDownloadContentRoot(const QString& downloadPath);

InstallAnalysis analyzeDownloadPath(const QString& downloadPath);
InstallAnalysis analyzeTorrentFileNames(const QStringList& fileNames);

} // namespace arachnel::core

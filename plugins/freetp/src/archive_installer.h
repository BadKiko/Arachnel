#pragma once

#include "windows_runner.h"

#include <QString>

namespace freetp {

QString findGameExecutable(const QString& rootDir);

QString findDownloadContentRoot(const QString& downloadPath);

bool extractArchivesInDirectory(const QString& downloadDir, const QString& destDir,
                                QString* errorOut);

QString installPortableFromDownload(const QString& downloadPath, const QString& targetPath,
                                    QString* errorOut);

bool installAddonOverlay(const QString& downloadPath, const QString& gameInstallPath,
                         QString* errorOut);

bool runInstallProcess(const QString& program, const QStringList& arguments, int timeoutMs,
                       QString* errorOut, const QString& workingDirectory = {},
                       const arachnel::core::WindowsRunEnv& env = {});

} // namespace freetp

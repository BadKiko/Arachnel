#pragma once

#include "windows_runner.h"

#include <QString>

namespace freetp {

QString findSetupExecutable(const QString& rootDir);

bool isInnoSetupExecutable(const QString& setupPath);

QString installInnoSetup(const QString& setupPath, const QString& targetPath, QString* errorOut,
                         const arachnel::core::WindowsRunEnv& env = {});

QString installInnoOverlay(const QString& setupPath, const QString& targetPath, QString* errorOut,
                           const arachnel::core::WindowsRunEnv& env = {});

void cleanupInnoSideEffects(const QString& installPath);

} // namespace freetp

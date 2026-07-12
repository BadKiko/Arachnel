#pragma once

#include <QString>

namespace freetp {

QString findSetupExecutable(const QString& rootDir);

bool isInnoSetupExecutable(const QString& setupPath);

QString installInnoSetup(const QString& setupPath, const QString& targetPath, QString* errorOut);

QString installInnoOverlay(const QString& setupPath, const QString& targetPath, QString* errorOut);

void cleanupInnoSideEffects(const QString& installPath);

} // namespace freetp

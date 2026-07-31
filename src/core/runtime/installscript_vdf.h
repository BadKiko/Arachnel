#pragma once

#include "runtime_dependency_types.h"

#include <QString>
#include <QVector>

namespace arachnel::core {

/**
 * Parse Steam installscript.vdf "Run Process" blocks.
 * process/command paths still contain %INSTALLDIR% until resolveInstallDirPlaceholders.
 */
QVector<RedistInstallStep> parseInstallScriptRunProcess(const QString& vdfText);

/** Replace %INSTALLDIR% (any slash style) with the Steamworks Shared absolute path. */
void resolveInstallDirPlaceholders(QVector<RedistInstallStep>* steps, const QString& installDir);

/** True when Wine/Proton prefix registry files mention HasRunKey as installed. */
bool prefixHasRunKey(const QString& prefixDir, const QString& hasRunKey);

} // namespace arachnel::core

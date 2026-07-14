#pragma once

#include <QString>

namespace arachnel::setup {

bool registerWindowsUninstall(const QString& installPath, const QString& uninstallExe,
                              const QString& displayVersion, QString* errorOut = nullptr);

bool unregisterWindowsUninstall(QString* errorOut = nullptr);

} // namespace arachnel::setup

#pragma once

#include "install_kind.h"

#include <QString>

namespace arachnel::core {

struct InstallAnalysis {
    InstallKind kind = InstallKind::PortableArchive;
    QString methodId;
    int confidence = 0;
    QString detail;
    bool canInstall = false;
};

InstallAnalysis makeInstallAnalysis(InstallKind kind, const QString& methodId, int confidence,
                                    const QString& detail, bool canInstall);

} // namespace arachnel::core

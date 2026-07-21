#include "install_analysis.h"

namespace arachnel::core {

InstallAnalysis makeInstallAnalysis(InstallKind kind, const QString& methodId, int confidence,
                                    const QString& detail, bool canInstall)
{
    InstallAnalysis analysis;
    analysis.kind = kind;
    analysis.methodId = methodId;
    analysis.confidence = confidence;
    analysis.detail = detail;
    analysis.canInstall = canInstall;
    return analysis;
}

} // namespace arachnel::core

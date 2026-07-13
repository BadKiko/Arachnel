#include "job_status.h"

#include <QCoreApplication>

namespace arachnel::core {

QString jobStatusLabel(const QString& status)
{
    if (status == QStringLiteral("queued"))
        return QCoreApplication::translate("Core", "Queued");
    if (status == QStringLiteral("starting"))
        return QCoreApplication::translate("Core", "Starting");
    if (status == QStringLiteral("checking"))
        return QCoreApplication::translate("Core", "Checking");
    if (status == QStringLiteral("metadata"))
        return QCoreApplication::translate("Core", "Metadata");
    if (status == QStringLiteral("downloading"))
        return QCoreApplication::translate("Core", "Downloading");
    if (status == QStringLiteral("installing"))
        return QCoreApplication::translate("Core", "Installing");
    if (status == QStringLiteral("seeding"))
        return QCoreApplication::translate("Core", "Seeding");
    if (status == QStringLiteral("paused"))
        return QCoreApplication::translate("Core", "Paused");
    if (status == QStringLiteral("completed"))
        return QCoreApplication::translate("Core", "Completed");
    if (status == QStringLiteral("failed"))
        return QCoreApplication::translate("Core", "Failed");
    if (status == QStringLiteral("cancelled"))
        return QCoreApplication::translate("Core", "Cancelled");
    return status;
}

QString jobDisplayStatusLabel(const QString& status, const QString& detail)
{
    if (status == QStringLiteral("completed") && isJobInstallFailed(detail))
        return QCoreApplication::translate("Core", "Install failed");
    return jobStatusLabel(status);
}

} // namespace arachnel::core

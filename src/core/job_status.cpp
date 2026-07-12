#include "job_status.h"

#include "i18n.h"

namespace arachnel::core {

QString jobStatusLabel(const QString& status)
{
    if (status == QStringLiteral("queued"))
        return trCore("Queued");
    if (status == QStringLiteral("starting"))
        return trCore("Starting");
    if (status == QStringLiteral("checking"))
        return trCore("Checking");
    if (status == QStringLiteral("metadata"))
        return trCore("Metadata");
    if (status == QStringLiteral("downloading"))
        return trCore("Downloading");
    if (status == QStringLiteral("installing"))
        return trCore("Installing");
    if (status == QStringLiteral("seeding"))
        return trCore("Seeding");
    if (status == QStringLiteral("paused"))
        return trCore("Paused");
    if (status == QStringLiteral("completed"))
        return trCore("Completed");
    if (status == QStringLiteral("failed"))
        return trCore("Failed");
    if (status == QStringLiteral("cancelled"))
        return trCore("Cancelled");
    return status;
}

QString jobDisplayStatusLabel(const QString& status, const QString& detail)
{
    if (status == QStringLiteral("completed") && isJobInstallFailed(detail))
        return trCore("Install failed");
    return jobStatusLabel(status);
}

} // namespace arachnel::core

#include "crash_log.h"
#include "crash_log_internal.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>

namespace arachnel {

bool hasPendingCrashReport()
{
    return QFileInfo::exists(pendingCrashMarkerPath());
}

QString pendingCrashSummary()
{
    return readPendingField(QStringLiteral("summary"));
}

QString pendingCrashDetails()
{
    const QString path = pendingCrashReportPath();
    if (!path.isEmpty()) {
        const QString details = readTextFile(path);
        if (!details.isEmpty())
            return details;
    }
    return readPendingField(QStringLiteral("details"));
}

QString pendingCrashReportPath()
{
    const QString path = readPendingField(QStringLiteral("reportPath"));
    if (!path.isEmpty())
        return path;
    return latestCrashReportPath();
}

QString pendingCrashIssueUrl()
{
    return readPendingField(QStringLiteral("issueUrl"));
}

void dismissPendingCrashReport()
{
    removePendingMarker();
}

void openPendingCrashIssue()
{
    const QString summary = pendingCrashSummary();
    const QString details = pendingCrashDetails();
    // Rebuild so older pending markers (pre-fence) still get a safe GitHub body.
    const QString url = !summary.isEmpty() && !details.isEmpty()
                            ? buildIssueUrl(summary, details)
                            : pendingCrashIssueUrl();
    if (!url.isEmpty())
        QDesktopServices::openUrl(QUrl(url));
}

void revealPendingCrashReport()
{
    const QString path = pendingCrashReportPath();
    if (path.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

} // namespace arachnel

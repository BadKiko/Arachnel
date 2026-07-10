#pragma once

#include <QString>

namespace arachnel::core {

inline bool isJobQueued(const QString& status)
{
    return status == QStringLiteral("queued");
}

inline bool isJobActive(const QString& status)
{
    return status == QStringLiteral("checking") || status == QStringLiteral("metadata")
           || status == QStringLiteral("downloading") || status == QStringLiteral("seeding")
           || status == QStringLiteral("starting");
}

inline bool isJobPaused(const QString& status)
{
    return status == QStringLiteral("paused");
}

inline bool isJobTerminal(const QString& status)
{
    return status == QStringLiteral("completed") || status == QStringLiteral("failed")
           || status == QStringLiteral("cancelled");
}

inline bool isJobInProgress(const QString& status)
{
    return isJobQueued(status) || isJobActive(status) || isJobPaused(status);
}

inline bool isJobRunning(const QString& status)
{
    return isJobActive(status) || isJobPaused(status);
}

QString jobStatusLabel(const QString& status);

} // namespace arachnel::core

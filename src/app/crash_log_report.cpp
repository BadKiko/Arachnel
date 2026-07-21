#include "crash_log.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QUrl>

#include <cstdio>
#include <vector>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#else
#include <csignal>
#include <cstring>
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <unistd.h>
#endif

namespace arachnel {

#include "crash_log_internal.h"

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
    const QString url = pendingCrashIssueUrl();
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

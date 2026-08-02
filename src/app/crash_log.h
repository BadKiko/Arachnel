#pragma once

#include <QUrl>
#include <QString>

namespace arachnel {

bool isCrashDialogMode(int argc, char* argv[]);
void markApplicationShuttingDown();
void installCrashLogging();
void logRunStarted(int argc, char* argv[]);
void logRunFinished(int exitCode);
void logDiagnostic(const QString& line);
/** Short trail for crash reports (catalog load, launch, …). */
void logBreadcrumb(const QString& where, const QString& detail = {});
void logQmlWarning(const QUrl& url, int line, int column, const QString& description);

bool hasPendingCrashReport();
QString pendingCrashSummary();
QString pendingCrashDetails();
QString pendingCrashReportPath();
QString pendingCrashIssueUrl();
void dismissPendingCrashReport();
void openPendingCrashIssue();
void revealPendingCrashReport();

} // namespace arachnel

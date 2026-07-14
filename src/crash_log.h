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
